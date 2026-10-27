# ICC RaidObs integration and cheat audit

In flight. Wires the Icecrown Citadel strategy (`src/Ai/Raid/ICC/`, map 631, strategy key `icc`) into
RaidObs, catalogues every bot cheat it uses, removes the simple ones and fixes small bugs on the way.
Work runs as waves of lanes, one brief per lane in this directory. `i6-closeout` deletes this
directory once its durable content lives in `docs/raids/icecrown-citadel/`. Fork-only: nothing here
is prepared for upstream.

## Status

The merge stage alone edits this table.

| Wave | Lane | State | Merge | Decisions for review | Carried over / gaps |
|---|---|---|---|---|---|
| 1 | i0a-tooling | merged | `3bd3925a3` | No `icc spike` prefix row (it is an action); a NOTE_PREFIXES row per contract prefix; `icc` matches contract slugs only; extra aliases `stinky`, `precious`; `iccbuffs` flagged only on heroic; `cheated none recorded` only on v14 ICC; `--cheats` targets as a set; ICC pulls can now get strategy-off | `verify.py` could check `cheat` `g`/`tgt`; `--bot`/`--notes` skip cheat rows; giant keys read silent under shared `icc.`; aliases not map-scoped (Utgarde Keep, Ahn'kahet princes) |
| 1 | i0b-foundation | merged | `4923506dd` | `MarkPull` takes a name; engage/mark opens read the queued boss id; a boss state renames a trace or closes a MarkPull one; reset 5 s after the last boss state ends (kill or wipe); struct values and edge detectors left bare; listener probes unpaced; cheat rows count real uses only | Boss lanes: replace `*.nitro` with `UseNitroBoosts`, Big Ooze pickup with the real taunt, drop `IccCastClassTaunt` threat/reset and `vd.speed`, update the README cheat rows; i2: unused `SPELL_MALLEABLE_GOO_*`, `fgSlotState`; i4: `vtCloudSync.targetCloudGuid`, `sgFlaredRedThisPhase`; gaps: icon targeting, humans keep Experienced |
| 2a | i1-lower-spire | merged | `9178073be` | Taunt on cooldown fails, no shoot/throw fallback; strip covers the bot and humans, Aegis/Pain Suppression left to expire; Deathwhisper phase moves to the Mana Barrier break (changes play); gunship radii kept, unstuck `NearTeleportTo` with `removeTransport`; no phase key for Saurfang or the giant, `gsb.phase` is Below Zero; all four gap cheats kept | i2–i5: every taunt loop reaches one unit per 8 s cooldown, no fixate (note per boss doc); gaps: icon targeting, Deathwhisper skull never leaves a stale non-add, kept gap cheats, `dbs.slot` writes `0` for slot 0 and erase, `HandleAddTargeting` uncalled |
| 2a | i2-plagueworks | merged | `5d888e2ba` | `pp.phase` from health thresholds, not `GetData`; phase and decision probes are per-bot `NoteDerived`; Spiteful Fury removed though the assist tank never attacks a Big Ooze; `pp.freeooze` kept as gap; icon probes only by the mover; Vile Gas spot resets after 2 s idle; Acid Burst fixed in i4's `ICCActions_VT.cpp` | `SPELL_NITRO_BOOSTS` stays for i4/i5, `SPELL_SPITEFULL_FURY` for the buffs; i6: shared ICC reader helper, `--phases` says "never written" on phaseless encounters; gaps: Big Ooze held by taunt threat alone, icon targeting, raid cheat off still stamps Regurgitated Ooze's cooldown, no answer to Decimate or Festergut's Vile Gas, `FindAssignedBigOoze` uncalled, nothing measured yet |
| 2b | i3-crimson-hall | pending | | | |
| 2b | i4-frostwing | pending | | | |
| 2c | i5-lich-king | pending | | | |
| 3 | i6-closeout | pending | | | Runs on the user's call, after ICC pulls |

States: `pending`, `merged`, `blocked` (branch and worktree kept, reason in the lane brief),
`skipped` (merge refused because the main tree had the file dirty; branch kept).

## Why

ICC is the one large raid RaidObs leaves bare. No ICC code calls RaidObs, and `IccInstanceState`
(`ICCScripts.h`) holds every cross-bot assignment in plain `std::map`/`std::set`, so a trace records
positions and verdicts but no decision. Several encounters file under the map name. ICC also cheats far
more than `docs/raids/README.md` suggests: 53 sites. Only 2 check `BotCheatMask::raid`, both in
`IccCastClassTaunt`. 12 depend on `Playerbots.EnableICCBuffs` (shipped 1, overridden to 0 on the
user's server) and 39 have no gate at all. `cfg.cheats` in a trace header is the conf string, so no
trace can say which cheat fired.

Verified constraints:

- **Boss state.** `instance_icecrown_citadel.cpp` calls `SetBossNumber(15)`; ids in
  `icecrown_citadel.h`: 0 Marrowgar, 1 Deathwhisper, 2 Gunship, 3 Saurfang, 4 Festergut, 5 Rotface,
  6 Putricide, 7 Blood Prince Council, 8 Blood-Queen, 9 Svalna, 10 Valithria, 11 Sindragosa, 12 Lich
  King, 13 Sindragosa gauntlet, 14 Blood Prince trash. Every one goes through `SetBossState`, so
  RaidObs already opens and closes ICC traces on boss state.
- **Trace names without help.**
  - Valithria (friendly), Svalna (not boss-flagged), the gauntlet and the Blood Prince trash keep the
    map name.
  - Gunship starts under the map name, then takes the captain's creature name (its credit is a spell).
  - Blood Prince Council files as "Blood Council" only when Valanar engages first. The credit entry is
    37970, so Keleseth or Taldaram first names the file after the prince.
  - Blood-Queen files as "Queen Lana'thel".
  - Stinky/Precious and the Rotting Frost Giants open no trace.
  - The world DB's `dungeonencounter_dbc` is empty, so the DBC encounter names are unconfirmed.
- **No gate.** `ICCStrategy.cpp` pushes 51 plain trigger nodes and 13 multipliers. Each trigger
  checks boss presence itself, so every encounter's nodes are asked during every other fight.
  `--coverage` can fold them only through the trigger-prefix table.
- **Spell listeners** in `ICCScripts.cpp` (Malleable Goo, Vile Gas, Defile) run on the caster's map
  thread, which satisfies RaidObs's threading rule.
- **State reset.** `IccBossStateResetScript` resets the whole instance state 5 s after an
  `IsDungeonBoss()` creature leaves combat, so Gunship, Valithria, Svalna, Keleseth and Taldaram never
  trigger it. `IccMapCleanupScript` resets on `OnDestroyMap`.
- No `631_` trace exists yet. `Playerbots.Obs.Maps` is empty, so every raid map records.

## Decisions

| Topic | Decision |
|---|---|
| Design questions | Never pause. The core script decides mechanics; the rules below decide scope. Log every judgement call under the brief's "Decisions for review" with its source. |
| Scope | Observability, cheat classification, simple-cheat removal, small bugs. No tactic rework, no ICC restructure: the code is upstream's, and splitting files would conflict with every upstream merge. |
| Trace names | ICC registers a boss-id → slug table with RaidObs, and a boss-state open on that map takes the slug outright. `MarkPull` covers Stinky/Precious and the Rotting Frost Giants. ICC gets no encounter gate. |
| Cheats | Simple ones go (see Cheat classification). Every kept cheat sits behind `BotCheatMask::raid` and writes a `cheat` record. The `EnableICCBuffs` buffs stay as an operator option and only record. |
| Bugs | Small bugs get fixed: local fix, settled by the core script or DBC, no tactic change. Anything bigger goes to the boss doc's Known gaps. |
| Core | Never edited. A "core script is bugged" claim gets checked and written into the boss doc, with a ready-to-file issue text when it holds. |
| Targeting | ICC marks targets with raid icons. Converting to direct targeting (the Ulduar idiom) is a tactic rework, out of scope: probe every icon pick and list icon targeting as a known gap. |
| Build and test | The user builds and pulls. Agents never build, and never start or stop a Docker container. |

## Naming contract

C++ emits and Python accepts exactly these. A lane never changes a row; the merge stage edits the
table when a fact disproves one.

| Encounter | Boss id | Trace slug | Note and cheat prefix |
|---|---|---|---|
| Lord Marrowgar | 0 | `lord-marrowgar` | `lm.` |
| Lady Deathwhisper | 1 | `lady-deathwhisper` | `ldw.` |
| Gunship Battle | 2 | `icecrown-gunship-battle` | `gsb.` |
| Deathbringer Saurfang | 3 | `deathbringer-saurfang` | `dbs.` |
| Festergut | 4 | `festergut` | `fg.` |
| Rotface | 5 | `rotface` | `rf.` |
| Professor Putricide | 6 | `professor-putricide` | `pp.` |
| Blood Prince Council | 7 | `blood-prince-council` | `bpc.` |
| Blood-Queen Lana'thel | 8 | `blood-queen-lana-thel` | `bql.` |
| Sister Svalna | 9 | `sister-svalna` | `ss.` |
| Valithria Dreamwalker | 10 | `valithria-dreamwalker` | `vd.` |
| Sindragosa | 11 | `sindragosa` | `sg.` |
| The Lich King | 12 | `the-lich-king` | `lk.` |
| Sindragosa gauntlet | 13 | `sindragosa-gauntlet` | `sg.` |
| Blood Prince trash | 14 | `blood-prince-trash` | `bpc.` |
| Stinky and Precious | — (`MarkPull`) | `stinky-and-precious` | `dogs.` |
| Rotting Frost Giant | — (`MarkPull`) | `rotting-frost-giant` | `rfg.` (was `icc.` until `i1-lower-spire`) |
| raid-wide | any | — | `icc.`, matches every ICC slug |

Each boss publishes its phase as `<prefix>.phase`.

**Schema v14**, additive (v13 stays in `READABLE_SCHEMAS`):

- `hdr.cfg.iccbuffs`: `Playerbots.EnableICCBuffs` as 0 or 1.
- Record `cheat`:
  - `g`: the bot whose code ran the cheat.
  - `tgt`: the unit it acted on, absent when that is the bot. `EnsureUnit` names it.
  - `k`: `<prefix>.<kind>`.
  - `n`: uses since this bot's last row for `k`.
  - Written on the first use per (bot, `k`) in a session, then at most once per 10 s per (bot, `k`),
    and flushed at the close.
  - API: `RaidObs::NoteCheat(Player* bot, char const* key, Unit* target = nullptr)`, gated inside on
    `Active()`. Raid-agnostic, so other raids can adopt it.
- ICC calls it through two helpers:
  - `IccRaidCheat(botAI, key, target)` returns `HasCheat(BotCheatMask::raid)` and records when true.
    Every cheat without an `EnableICCBuffs` gate goes behind it.
  - The `EnableICCBuffs` sites keep their own gate and only record.
- Fixed keys, used by verification: `<prefix>.nitro` (one per encounter), `rf.spitefulfury`,
  `icc.tauntfixate`, `icc.tauntreset`, `vd.speed`. `i0b-foundation` names the rest and lists them in
  the README cheat table.

## Cheat classification

A cheat is **simple** when its legit answer is something bots already do (walk, a real class taunt
through `IccCastClassTaunt`, a real item the bot carries), sits at the cheat site, and needs no new
duty, assignment or positioning design. Simple cheats are removed. Every other cheat stays behind
`raid`, records itself, and goes into its boss doc as an open gap, saying what a legit answer would
need. A lane may move a row between classes only with a rationale under "Decisions for review".

Line numbers are from `1496982cf` and drift; find a site by its code.

- **Remove (19 sites):**
  - Nitro Boosts `AddAura 54861` (13): RF:162,692,750,784,989; PP:733; VT:1089; SG trig:799,
    SG:614,796; LK:377,3565,4054. Each becomes `UseNitroBoosts(bot)` (`src/Util/EncounterHelpers.h`):
    the bot's real boots and their 3 min cooldown. Without boots, the bot walks. LK:4054's Pain
    Suppression and Aegis go with no replacement.
  - Spiteful Fury for Big Ooze pickup (3): RF:148,235,248. Replaced by the real taunt.
  - `IccCastClassTaunt`'s 1M threat plus fixate and its taunt-cooldown reset (2): ICCShared.cpp:60-91.
    Only the real taunt stays. Owned by `i1-lower-spire`.
  - Valithria Dream State speed ×2 (1): VT:928.
- **Keep, `EnableICCBuffs` (12):** LDW:133,136; PP trig:394,407; SG trig:775,790;
  LK:176,373,1810,1823,3833; `IccApplyHeroicBuffToMember`. These reach human players too.
  `i1-lower-spire` fixes `IccStripExperienced` so humans lose Experienced and 1% Threat after the
  fight as well.
- **Keep, gap (22):**
  - LDW: Dominate Mind Cyclone on self and humans (140,157); Touch of Insignificance strip (168).
  - Rotting Frost Giant: Death Plague strip (LDW:768, comment calls the core bugged).
  - Gunship: Battle Fury strip on the enemy captain (341, comment calls the core bugged).
  - PP: Volatile Ooze and Gas Cloud kills (521,680), Unbound Plague suicide (1327), Regurgitated Ooze
    with no energy cost (1555).
  - BQL: Swarming Shadows strip (26).
  - VT: teleports to a cloud and onto a Dream State player (962,1028).
  - SG: Frost Beacon strip (trig:809), Mystic Buffet strip (trig:815), Magic Barrier immunity
    (trig:821), 1% Threat on the assist tank (trig:845), smoke flares through `StoreNewItem` (492;
    documented, not removed, as with Mimiron's core), tomb aura strips (981,1064).
  - LK: Raging Spirit kill (382), Wicked Spirit kill (2420), Hammer of Justice from any class (2715).
- **Borderline, documented only:** VT:716,856 floor snap that skips fall damage; LK:1962-1991 Val'kyr
  drop suicide.
- **Not cheats:** spell clicks and object use in range, real item use, own-aura cancels (BQL:1144),
  the `ICCScripts.cpp` listeners.

Known small bugs, split by owner in the briefs:

- Single-difficulty spell ids: `SPELL_VILE_GAS_H` 69240, `SPELL_ACID_BURST` 70744,
  `SPELL_HARVEST_SOUL_LK` 68980, `SPELL_MALLEABLE_GOO_25N` 72297, `SPELL_MALLEABLE_GOO_10H` 74280.
- Same-map `TeleportTo` unstucks drop combat (`docs/engine/pitfalls.md`): BPC:29,559, GSB:200, LK:661,
  2043, 2075, 2087, 2146. Each becomes `NearTeleportTo`.
- Hard-coded heals cast spells the bot may not know (VT:885-915, SG:562), because
  `PlayerbotAI::CastSpell(uint32)` never checks. Add a known-spell check at those sites only.

## Lane rules

These bind every agent in every lane; a lane brief adds scope and facts.

**Authority.** The user chose this for the effort: "Each lane works on its own branch `icc/<lane>`
in a worktree under `build-icc-wt/`, two lanes at a time. One merge agent merges them into Custom
with `--no-ff` without waiting for you. Never push, amend, rebase or force. You only say 'go' for each
wave." For this effort only, that replaces `CLAUDE.local.md`'s "no branch". Allowed:
`git worktree add/remove`, commits on `icc/<lane>`, `git merge --no-ff` (merge stage only),
`git branch -d`. Never: push, amend, rebase, `reset --hard`, force, stash, `clean`, a checkout that
discards changes. Never ask the user anything; settle questions per Decisions and log them.

**Worktree.**

- Main tree: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-playerbots` on `Custom`.
- Lane worktree: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-icc-wt/<lane>` on `icc/<lane>`. Create
  it from the main tree with `git worktree add -b icc/<lane> <wt> Custom`. If the branch exists, use
  `git worktree add <wt> icc/<lane>`. If the worktree exists, reuse it and continue from its state.
- The core's `/build*/` gitignores the path. It stays out of `modules/`, so the core CMake doesn't
  build it as a second module. It sits two levels under the core root, so
  `tools/botobs/raidobs/paths.py` still finds the traces.
- Every edit of a lane happens in its worktree. Only the merge stage writes the main tree.

**Files a lane may touch.**

- Its brief's "Owns" list, plus its own boss regions of the shared files: `ICCTriggers.{h,cpp}`,
  `ICCMultipliers.{h,cpp}`, `ICCActions.h`, `ICCStrategy.cpp`, `ICCTriggerContext.h`,
  `ICCActionContext.h`.
- Anything else only where the brief says so, listed in the report.
- Never this file, `src/Ai/Raid/Uld/` or `src/Ai/Raid/ToC/`.
- `docs/raids/icecrown-citadel/README.md` belongs to the merge stage once `i0b-foundation` has
  created it.

**Sources.**

- Mechanics: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/src/server/scripts/Northrend/IcecrownCitadel/`.
  Follow a call to its guards before relying on its effect.
- Spell ids per difficulty:
  `G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-spell-tweaks/data/dbc-reference/spelldifficulty.reference.csv`
  plus the world DB:
  `docker exec ac-database mysql -uroot -ppassword -N -B -e "SELECT … FROM acore_world.spelldifficulty_dbc …"`.
  Spell fields are in `spell.reference.csv` next to the CSV.
- Coordinates: navprobe only, per `docs/engine/pitfalls.md`. A lane in this effort should rarely
  need a new one.

**Required reading before designing.** `docs/engine/pitfalls.md`, `raid-mechanics-lessons.md`,
`action-selection.md`; `docs/raids/README.md`; `docs/systems/observability.md` ("Adding a probe",
"Reading traps", "What a trace cannot tell you"); `docs/raids/icecrown-citadel/README.md` once it
exists. Probe examples: `src/Ai/Raid/ToC/Util/ToCHelpers_Jaraxxus.cpp` (containers in a
`RaidInstanceState`), `docs/raids/ulduar/mimiron.md` ("What a trace answers").

**Probe rules.**

- Probe the rule, not the coordinate.
- Probe inside the helper that derives a value, never at its call sites.
- Nothing that repeats the act stream.
- Every declared key can emit.
- Every key carries its encounter's contract prefix.
- Timestamps, throttles and caches stay bare.
- RaidObs calls sit only in ICC `.cpp` files and `src/Bot/Obs/`.

**Checks**, from the worktree root:

- `PB_REPO=<wt> PB_MAX_FANOUT=60 ~/.claude/scripts/pb-syntax-check.sh <changed .cpp and .h>`, plus the
  lane's own `.cpp` files in a run of their own (a widely included header fills the cap first).
- `python tools/pblint/pblint.py --warnings src/Ai/Raid/ICC src/Bot/Obs src/Bot/PlayerbotAI.cpp src/Ai/Raid/RaidStrategyContext.h`,
  plus the same paths with `--spell-difficulty`. Stay scoped: the whole tree carries known errors.
- `python -m unittest discover -s tools/botobs/tests`.
- `python apps/codestyle/codestyle-cpp.py`. It scans all of `src`, so a failure in an untouched file
  isn't the lane's.
- In a split lane only the finisher runs the checks.

**Commit.** One commit per lane on `icc/<lane>`, staged by explicit path, never `-A`. Subject ≤72
characters; body at most three lines, only a why the diff can't show; voice per
`use-conversational-language`; no AI attribution or `Co-Authored-By`. Code comments follow the
no-nonsense-comments rule: no process narration, no lanes, waves or plans.

**Docs.**

- Durable facts go to `docs/raids/icecrown-citadel/<slug>.md` per `docs/README.md`: ids, timers,
  decisions with rationale, traps, known gaps, no change lists.
- Each boss doc has a "Cheats" section (removed, kept and why, what a legit answer needs) and ends
  with "What a trace answers" (probe keys, cheat keys, reader flags).
- A generic engine lesson goes into the lane report for the merge stage.
- Fold docs with `compact-docs-writer` (standing approval in `CLAUDE.local.md`).

**Boss lane steps.** Each wave-2 lane does these for every encounter in its brief:

1. **Phase.** `<prefix>.phase` from one helper that every phase test of the boss calls, derived from
   the core's thresholds. Where sites disagree, unify them on the core's value as a small-bug fix and
   log it.
2. **Decisions.**
   - `NoteDerived` inside each helper that derives an assignment per call, and for the struct values
     `i0b-foundation` left bare.
   - Probe every raid-icon pick (which unit the icon went on, by rule).
   - Hazard notes in the helpers that model a hazard with no world object.
   - Readers consume these; they never re-derive them.
3. **Cheats.** Remove the brief's simple cheats. Recheck its gap cheats for a simple answer. Keep each
   kept site's `IccRaidCheat` wrapper.
4. **Bugs.** Fix the brief's small bugs and any others found that meet the rule.
5. **Reader.** `tools/botobs/bosses/<slug_with_underscores>.py` per contract slug, shaped like
   `bosses/general_vezax.py`:
   - banner, refusing other encounters;
   - phase timeline;
   - assignments;
   - this encounter's cheat uses.
   Tests go in `tests/test_<slug_with_underscores>.py` on a synthetic pull, plus an `EveryView` check
   against the full fixture.
6. **Doc.** `docs/raids/icecrown-citadel/<slug>.md` per the Docs rule. Icon targeting goes under
   Known gaps.

**Report.** Besides the wave script's fields:

- `cheatsRemoved` and `cheatsKept`, one line each with its key;
- `bugsFixed`;
- `cmakeRerun` true when the lane added a `.cpp` (the core globs `src/**`).

## Waves

| Wave | Lanes, two at a time |
|---|---|
| 1 | `i0a-tooling`, `i0b-foundation` |
| 2a | `i1-lower-spire`, `i2-plagueworks` |
| 2b | `i3-crimson-hall`, `i4-frostwing` |
| 2c | `i5-lich-king` |
| 3 | `i6-closeout` |

Wave 1 changes no play on the default config. The exceptions are the trigger-name fixes and the
state-reset fix in `i0b-foundation`. A pull between wave 1 and wave 2 is a baseline for which cheats
fire, and the boss lanes read it if it exists. It is optional. Each sub-wave merges before the next
starts, so later lanes branch from a `Custom` that already holds the earlier ones.

A lead agent runs each lane: investigate → implement → check → review the change (mechanics,
conformance, observability and conventions) → fix → close and commit. Only a big lane splits: the
lead hands its Task list to up to 3 implementers on disjoint files, and one finisher integrates,
checks, reviews, fixes and commits. A merge stage then lands every finished lane on `Custom`, runs the
checks on the merged tree, updates the status table and the README boss table, and removes merged
worktrees and branches.

Traces for a lane live in `G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/631_*.ndjson`.
Check `hdr.bin` against the commit under test before reading one as evidence
(`docs/systems/observability.md`, "Is this trace evidence?").
