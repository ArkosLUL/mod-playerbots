# ICC rebuild

In flight. Rebuilds the Icecrown Citadel strategy (`src/Ai/Raid/ICC/`, map 631, strategy key `icc`) to
this fork's conventions on 10N/25N/10H/25H: the per-boss layout ToC and Ulduar use, encounter gates,
direct targeting, duties instead of family vetoes, verified coordinates. Bot cheats are replaced by real
handling of each mechanic, and tactics come from guides. Work runs as waves of lanes, one brief per
lane in this directory. `r14-closeout` deletes this directory once its durable content lives in
`docs/raids/icecrown-citadel/`. Fork-only. Supersedes `docs/plans/icc-raidobs/` (its `i6-closeout`
never runs; `r1-foundation` folds and deletes it).

## Status

The merge stage alone edits this table.

| Wave | Lane | State | Merge | Decisions for review | Carried over / gaps |
|---|---|---|---|---|---|
| 1 | r0a-restructure | merged | `18bf92d57` | one generator, no split; pass-through gate never live; ids and gate in `ICCEncounterGate.h`; Dogs/FrostGiant `ICC_BOSS_NONE`=255; `Node<T, A>` with `Name`; root owns multiplier order from wave 3 | pure move, every old behaviour issue live; never built, CMake re-run needed; boss docs' `Code:` lines name pre-split files; dead code for r3 (Rotting Giant TODO) and r7 (`spellEntriesFlood`, commented multiplier); r1 only swaps `IccPassThroughGate` |
| 1 | r0b-guides | merged | `9be681d7f` | noobtoboss refused (robots.txt); `zamimg.com` refused like Wowhead; overview slug `icecrown-citadel`; per-source sections; hidden tab content kept | Wowhead: 12 boss pages saved and converted, overview not saved; no Crimson Hall trash guide; r3, r4, r11-r13 briefs still list noobtoboss; r14: toc-rework plan's Wowhead reason is wrong |
| 2 | r1-foundation | merged | `6c1bd0f05` | other encounters' nodes close while one is in progress, multipliers live-only but Festergut's and Rotface's (state moves ~7 s in); one `HookRows()` table, first live row answers; opener and aura triggers silent without a hook; raid-wide rows at +2.5/+1.5/+0.5; ToC's Anub'arak files untouched behind three aliases; no config-key pattern in `probes.py` | never built, CMake re-run (11 .cpp added, 4 ToC .cpp deleted); hooks miss the pre-live window (r6, r7, r11); unit gate closed in an unloaded grid; pet-opened Blood Prince trash pull untraced until a player is hit; no ICC seam user yet; r14: drop the ToC aliases, ToC boss docs' pre-hoist names, `probes.py` config-key pattern |
| 3a | r2-marrowgar | merged; review fixed 1 bug, 6 minor, 2 cleanups | `73f5d3907`, review `f2c545850` | non-baits within 3 yd of his centre, inner bots on a 160° arc of his back; 3/8 baits on a 31 yd ring, hunters first; all tanks 2 yd beside the MT; storm: fixed stack + anchors 34 yd at 180/60/300, every tank on 180 by the tank spot, nearest-slot baits on 60/300, moving on the 3 s cast, melee hold; MT re-centres him on his spawn after each storm; tanks dodge Coldflame; old `cross`/`star` `rti` reset to skull once per instance; no pre-pull staging; taunt-back left to class nodes | never built, CMake re-run (new helpers .cpp); no 631 trace; LOS unchecked (navprobe has no `los`); no Berserk pacing, storm cover or arrival defensive; no Hand of Protection on Impaled; heroic casters reaching a spiked anchor walk his lanes; humans hold no duty; dodge walk not path-tested (lines radiate); r14: unused `NPC_SPIKE*` ids in `ICCData.h`, hoist one walk base from `IccLmWalkAction` |
| 3a | r3-deathwhisper | merged; review fixed 3 bugs, 7 minor, 2 cleanups | `0f297100f`, review `642e329da` | tanks own a side by her y, no torch stands or P2 drag; no opening positions (23 yd aggro); kill order by damage school; CC Cyclone first, no tank CC; CC and kick picks form-checked (no cat or Tree druid on Cyclone); splash held 15 yd round an uncycloned victim; normal swap at 3 stacks by a stack-free taunter (lands ~4-5 stacks every 30 s), generic taunts blocked; lust held to P2 on all difficulties; one forced dodge; giant pairs round each carrier, partners walk to tank or player carriers | never built, CMake re-run (2 helper .cpp); no 631 trace; Cyclone recast gap (victim free ~1.5-2 s, immune target refused); Deformed Fanatic face-tanked; no heroic P2 DPS threat brake or Divine Shield drop; Deathchill Bolt and Curse of Torpor generic; slowed shade victim outrun; giant: melee uptime loss, unpartnered carrier dies; no `-Werror` pass |
| 3b | r4-gunship | merged; review fixed 1 bug, 3 minor, 2 cleanups | `3aa38a4e2` | `gsb.battlefury` replaced by return on the mage's death (tank last, 10 s cap) and a retreat at +200% Battle Fury (20 stacks normal, 17 heroic); fall snap dropped for the core rescue; bot roster (board/home tank, ranged on cannons, home healers 2/1, melee board); boarders jump once a tank holds him or 15 s in; edge slots assume the cannon side faces the enemy; dodges on a deck-box sweep, heroic interrupts; no Bladestorm dodge (radius 0); Burning Pitch dodge kept though its faction-35 caster may hit nobody | rebuild needed; no 631 trace; deck points can't be navprobed (transport floors); rocket-pack fix unproven live; no tank defensives against stacks; mortars only killable by cannons; humans hold no duty; no `-Werror` pass; r14: unused `gsbLastStarPos`/`gsbLastRocketPackUse`, `SPELL_FROZEN_CANNON` 69704 not in the DBC, `RaidRestoreLegacyRti` skips `star`, hoist one walk base |
| 3b | r5-saurfang | merged; review fixed 6 minor | `42e99f6d7` | tanks east, raid spread west on 11 slots 13.5 yd apart; elected swap by the rune-free tank, generic taunts blocked; rune tank yields to beasts; free tank's beast taunt gated on the next swap; ranged on the furthest beast within 32 yd; stuns and snares only; mark healer order holy paladin first; lust on Frenzy; area damage held for DPS all pull; slot holds block heal reach | CMake re-run (new helpers .cpp); no 631 trace; slot LOS unchecked; no Hand of Protection, Blood Power defensives or knockbacks; human tank not told to swap; Nova can pick the melee stack when slots are short; Thick Hide cat counts as a tank; r14: unused `NPC_BLOOD_BEAST1-4`, `IccTryClassCC` roots for Valithria, hoist one stand-walk base |
| 3ab-gaps | g2-marrowgar | merged; review fixed 4 minor, 1 cleanup | `7ede659ac` | tank buttons at each Bone Slice window and under 40%, class buttons held to 25%, human tank called; storm cover through `GetRaidCover` on 6 s windows of three ticks at a flat 28 yd; storm spike picks within 10 yd melee / 28 yd ranged; dodge walk refuses a straight step across another patch; all 10 bait slots kept (reviewer's re-probe overturned the cut to 8); no Hand of Protection (Impaled ignores immunities); no Berserk pacing | rebuild needed, no CMake re-run; no 631 trace; no `-Werror` pass; humans hold no duty; heroic spike on an anchor holder waits for healers or the storm's end; first storm tick never covered; cover damage is one flat 28 yd tick; no gameobject collision in sight; r14: unused `NPC_SPIKE*` ids, hoist one walk base |
| 3ab-gaps | g3-deathwhisper | merged; review fixed 5 minor, 2 cleanups | `1bc56a31a` | instant follow-up CC (Hammer of Justice, Repentance, Blind) in Cyclone's last second; druids and shamans drop a blocking form to join the Dominate Mind duty; Deformed Fanatic gets a tank defensive, no bounce; Frostbolt defensive when unkicked; threat brake at 90%/80% of the top tank on every difficulty; no Divine Shield; dodge spots must see the target; giant pairs walk out to a swept spot, tank as last-resort partner; slowed shade victim closed by analysis | rebuild needed, no CMake re-run; no 631 trace; no `-Werror` pass; brake blind until a tank has threat on her; no Fanatic bounce; Cyclone gap stays without instant CC; giant: a pair with a tank or player still displaces others; Darnavan outside the kill order |
| 3ab-gaps | g4-gunship | merged; review fixed 4 minor | `f26ffcfc1` | board tank button at +50% and +100% of the captain's hit, home tank by wave size or rank, any tank under 40%, class nodes held to 25%, human tank called; edge slots stay (cannon side faces the enemy on all four stops); no burst pacing; mortars stay with the cannons | rebuild needed, no CMake re-run; no 631 trace; no `-Werror` pass; thresholds are guesses until `--defensives` shows a pull; no deck coordinate or sight checkable offline; freeze mage keeps personal burst held (needs an entry exemption in the shared gate); home tank latch can re-open on a flapping wave; `BuildSharedStrategyContexts.cpp` fails the syntax check on a `TOCStrategy.h` case mismatch (RaidStrategyContext.h:29); r14: unused `gsbLastStarPos`/`gsbLastRocketPackUse`, `SPELL_FROZEN_CANNON`, `RaidRestoreLegacyRti` skips `star` |
| 3ab-gaps | g5-saurfang | merged; review fixed 2 minor, 1 cleanup | `24408c317` | slots 10 and 11 moved from 33 yd north/south to the east causeway (blind to every tank spot); tank ladder at Blood Power 70 (40 with a Mark) and through Frenzy, a Mark carrier counts as holder, under 40%, class nodes held to 25%; marked bots use `GetRaidPersonalDefensive`; Hand of Protection only on caster-type marked bots; melee fill of free inner slots; human tank called (button and taunt); Frost Trap, Earthbind, knockback, kiting, Divine Intervention recorded as gaps; Thick Hide cat bullet dropped per user ruling | rebuild needed, no CMake re-run; no 631 trace; no `-Werror` pass; a lethal-looking Mark can spend an immunity or healthstone (`icc.personal` shows how often); no external healer cooldowns; human tank call shares one 6 s window for taunt and button; slot 10 about 0.25 yd past the Nova ring; r14: unused `dbsBotSlotMemory`, `NPC_BLOOD_BEAST1-4`, hoist one stand-walk base |
| 3c | r6-festergut | pending | | | |
| 3c | r7-rotface | pending | | | |
| 3d | r8-putricide | pending | | | |
| 3d | r9-blood-prince-council | pending | | | |
| 3e | r10-blood-queen | pending | | | |
| 3e | r11-svalna-valithria | pending | | | |
| 3f | r12-sindragosa | pending | | | |
| 3g | r13-lich-king | pending | | | |
| 5 | r14-closeout | pending | | | |

States: `pending`, `merged`, `blocked` (branch and worktree kept, reason in the lane brief),
`skipped` (merge refused because the main tree had the file dirty; branch kept). `refine-<boss>`
rows are added as the user's pulls brief them.

## Why

The RaidObs effort (waves merged up to `824468439`) gave every ICC encounter traces, probes, a reader
and a boss doc, and removed the simple cheats. The strategy is still upstream's code, and it repeats
the traps `docs/engine/` records. Counts as of `824468439`:

- flat files (`ICCTriggers.cpp`, `ICCMultipliers.cpp`, `Action/ICCActions_<stem>.cpp`), with LK 4454
  lines and PP 1698;
- no encounter gate: 51 trigger nodes and 13 multipliers are asked in every fight, and each trigger
  finds its boss by name (108 `"find target"`, 60 `FindNearestCreature`);
- 78 raid-icon writes (SG 24, LK 19, LM 12, VT 7, PP 6, GSB 5, BPC 2, LDW, RF, DBS 1);
- all 13 multipliers veto by `dynamic_cast`, several "everything but X". Rotface's calls `AttackStop`
  inside `GetValue`, and a Putricide branch can never be true;
- `FleePosition` in VT and hand-rolled flee vectors in LDW, BQL, FG, PP and LK;
- 13 triggers act inside `IsActive` (the Experienced strip in 9, the heroic buffs, Sindragosa's cheats);
- 106 named `ICC_*` positions plus inline ones, none navprobe-checked;
- `IccInstanceState` is its own store rather than `RaidInstanceState` (both lock the lookup only and
  rely on one map updating on one thread);
- 22 `IccRaidCheat` sites with 20 keys, plus four borderline snaps.

## Decisions

| Topic | Decision |
|---|---|
| Design questions | Never pause, never ask the user. The core script decides mechanics (ids, timers, radii, targeting). The guides decide tactics (kill order, positioning, assignments). On a conflict the script wins and the boss doc says why. What neither settles is decided conservatively. Log every such choice under the brief's "Decisions for review" with its source. |
| Layout | Restructure like ToC and Ulduar: per-encounter stems, an encounter gate, one definition per encounter with node rows and rules. The ICC README's old rule to keep changes inside upstream's files is void. |
| Upstream | During an upstream merge, ICC keeps ours. The merge report lists upstream's ICC changes so the user can decide which to port. |
| Scope | Every mechanic on 10N/25N/10H/25H, heroic-only included, for the 12 bosses plus Stinky and Precious, the Rotting Frost Giants, the Svalna and Sindragosa gauntlets, Rimefang and Spinestalker. Ordinary trash stays on the generic AI and is still traced. |
| Cheats | Each cheat is replaced by the guide's answer. One survives only when bots can't carry that answer out at all: behind `BotCheatMask::raid` through `IccRaidCheat`, recorded, with the reason in the boss doc and under "Decisions for review". A cheat comes back only on the user's call; bots failing at a legit answer get a refine lane. The borderline snaps fall under the same rule. |
| Heroic buffs | `Playerbots.EnableICCBuffs` stays as the operator option, with the same behaviour, recorded through `IccNoteCheat`. |
| Humans | 0 to 2 in any role. They hold no duty. A player's skull wins over every resolver. |
| Targets | A lane's "targets for the next pull" are written for 25N first, then 25H. |
| Pre-pull | Each boss lane places its own opening positions inside its definition. No raid-wide staging. |
| Guides | Warcraft Tavern 25 is the base, scaled to 10 by the script's 10-man values. Wowhead pages the user saved and the backups add detail. No agent fetches wowhead.com: its robots.txt disallows `anthropic-ai`, `Claude-Web` and `ClaudeBot`. Sites that answer scripts with 403 (Icy Veins, the wikis) are not worked around. |
| Build and test | The user builds and pulls. Agents never build. |

## Naming contract

The trace slugs, boss ids and note/cheat prefixes are the table in
`docs/raids/icecrown-citadel/README.md` ("Boss states and trace names"). C++ emits and the Python
readers accept exactly those. A lane never changes a row; the merge stage edits it when a fact
disproves one. Trigger names keep their `icc <boss>` form.

## Conventions

These bind every rebuilt encounter. Sources: **R** `docs/raids/README.md`, **P**
`docs/engine/pitfalls.md`, **L** `docs/engine/raid-mechanics-lessons.md`, **A**
`docs/engine/action-selection.md`, **O** `docs/systems/observability.md`, **C**
`docs/systems/consumables-and-burst.md`, **T** `docs/raids/trial-of-the-crusader/README.md`.

1. One `Definition/ICCDefinition_<Stem>.cpp` per encounter. Row classes carry a `static constexpr Name`. (R "File layout and wiring")
2. Everything sits behind the encounter gate, keyed on the fight (`BossStateGate` by boss id), not on a unit. (R "File layout and wiring")
3. A config read, a guid lookup or a hand-pushed multiplier is not a gate. (P "Movement that silently no-ops")
4. Housekeeping (notes, latch writes, wipe resets, buffs and strips) goes in the definition `Tick`, cheap checks first, never in a predicate or `IsActive`. (R "File layout and wiring")
5. Direct targeting: one resolver per encounter, an action on the direct-attack base, `OwnTargeting` with predicate "resolver returns a unit". No raid icons: bots never set one, for bots or for humans, and never steer through `rti` or `rti cc`. CC duties cast directly on their target. (T "Targeting is direct, no raid icons"; R "Targeting: the whole fight")
6. Each resolver takes a player's skull first: alive, in combat, selectable. It is the only icon a bot reads. (T)
7. Every resolver branch returns an in-combat unit: `Attack()` has no out-of-combat guard and reaches 100 yd. (P "Before the pull, and out of combat")
8. The direct-attack action commands the pet before its early return; the trigger also fires while the pet is off the pick. (A "Multiplier semantics")
9. Every targeting veto is paired with a node that assigns a target. `raid healer engine` keeps targetless healers working. (A; R)
10. Mid-pull retargeting clears `prioritized targets`. A bot `rti` setting the old code changed is reset once, as ToC's `ToCRestoreLegacyRti` does. (P "A targeting mark never clears inside a pull")
11. Movement is held with `OwnMovement` rules and `EncounterRow::Mover` rows, never blanket `MovementAction` vetoes (`AttackAction` is one). `ReachHeal` where healers must walk. Spell movers need a `Block`. (R "Encounter rules"; L "Movement that oscillates")
12. Never zero `ReachTargetAction` whole; block `reach melee`/`reach spell` by name. (P)
13. One mover per bot or vehicle, and it never fails silently (off-mesh `MoveTo`). (R "Movement: the only mover left"; L "Vehicles and flight")
14. No `FleePosition` dodges. Use `FindNearestPositionClearOfHazards` with clearance = hazard radius + park tolerance + margin. (P; L geometry rules)
15. Latched stands: park within 1 yd, walk back past 2.5 yd, return true while the walk is in flight, cap the latch, block generic movers while it holds. (L)
16. Predicates are pure reads. Split latches by edge. Trigger and action call one shared helper. (L "Coordinating a raid with no shared state")
17. Reach, then hold with hysteresis, so casters can cast. (P "A moving bot cannot cast")
18. A dodge that must not miss interrupts the cast first (`InterruptNonMeleeSpells(true)` when `IsMovementPreventedByCasting`). Judge a dodge by measured displacement. (P "A bot mid-cast cannot be moved at all")
19. Emergency dodges at `ACTION_EMERGENCY + 6` with `MOVEMENT_FORCED`. Settle precedence between forced dodges in multipliers. Clear a stalled `last movement`. (R "Priority conventions"; P)
20. Stagger priorities with `ACTION_RAID + N`, no ties: insertion order is behaviour. (R; T "Code layout")
21. Remap spell ids per difficulty at the call site (`sSpellMgr->GetSpellIdForDifficulty(SPELL_X, unit)`). Check the CSV and the world DB, boss casts included. Run `pblint --spell-difficulty`. (P "Detection that never fires"; R "Heroic and 10/25")
22. Read raid size from the map (`IsRaid25Man`, `GetRaidMapDifficulty`), never from the player. (P)
23. Never key on an instant's cast; read the aura, the script's guid slots or a timer model. (P; L "Reading a boss script")
24. `"find target"` only when the bot is surely on the threat list; otherwise resolve by entry or instance guid. (R "Boss helpers"; P "Things the bot cannot see")
25. Mirror core enums with a source comment and re-check them after every core merge. (L; P)
26. Cross-bot state lives in `RaidInstanceState<State>` (`Find` for has-state checks), never `thread_local`. (R "Per-instance state")
27. An encounter that writes a shared AI value resets it for the next one. (P "State does not cross objects")
28. Coordinates: navprobe, reading the settled Z, `--nav 0x09`, `los` where sight matters; hazards checked as a separate step; re-verified against traces. A gameobject floor or transport deck can't be checked offline: say so in the doc. (P navprobe bullets)
29. No cheats unless the Cheats decision above keeps one. (R "Raid cheats are on by default")
30. Never `ChangeStrategy` from encounter code; cast directly and add a hold. (R "Anti-fear"; A "Strategy assignment")
31. Redirects through `RaidRedirectThreatAction`; vetoes cast to concrete classes only. (R "Threat redirect — the rubric")
32. Keep `dynamic_cast` narrow or match `getName()`; a multiplier tests the action before any lookup. (A; L "What a strategy costs per raid")
33. Cost: per-bot `getMSTime` scans, instance guid caches, `EncounterTriggerPassId`. Never pass 2-99 as a trigger interval (it means seconds). (L)
34. Burst: register names in `IsBurstCooldownAction`; per-boss windows AND with the base gate; `BossHasNoStableVictim`; reset `holdState`. (C "The burst gate")
35. Splash holds through `SpellSplashes`, not the Aoe threat type. (L "Crowd control and threat on adds")
36. Taunt only the adds the encounter owns, set the target with the taunt, mind taunt DR. (R; L)
37. Resolve hyphenated or renamed spells by id. (P "Names fail silently at runtime")
38. Probes: `ObsGuidMap`/`ObsGuidSet`/`ObsValue`, `NoteDerived` inside the deriving helper, probe the rule not the coordinate, keys carry the contract prefix, `MarkPull` for unscripted pulls. (O "Adding a probe")
39. Food and drink blind a bot: veto them near hazards. Guard tank druids from caster form in holds that run out of combat. (P "Before the pull…")
40. Wiring order is veto credit order: definition rules go after their stem's multipliers. (A; T "Code layout › Seams")
41. Header names unique ignoring case. (A "Registration")
42. One code path for 10/25 N/H. Judge a change on raid damage too.
43. Tank defensives: each tank spends its cooldowns on the boss's predictable big hits and stack peaks through `NextTankDefensive` (`src/Ai/Raid/RaidTankDefensive.h`; `physicalOnly` where Anti-Magic Shell does nothing), with the class defensive nodes held off those buttons (`IsHeldTankDefensive`) until that window. `RaidHumanTankCall` tells a human tank; that is not a duty. (`docs/raids/ulduar/mimiron.md` Plasma Blast; `docs/raids/onyxias-lair.md`; T `anubarak.md`)

## Lane rules

These bind every agent in every lane; a brief adds scope and facts.

**Authority.** The user chose ToC-style git for this effort: each lane on branch `iccr/<lane>` in a
worktree under `build-icc-wt/`, two lanes at a time, merged into `Custom` with `--no-ff` by a merge
agent without waiting for them; they only say "go" per wave. For this effort only, that replaces
`CLAUDE.local.md`'s "no branch". Allowed: `git worktree add/remove`, commits on `iccr/<lane>`,
`git merge --no-ff` (merge stage only), `git branch -d`. Never: push, amend, rebase, `reset --hard`,
force, stash, `clean`, a checkout that discards changes.

**Docker and network.**

- Never start, stop or restart an existing container. Allowed: read-only `docker exec` queries, and
  navprobe's throwaway container (`docker run --rm -v azerothcore-wotlk-pb_ac-client-data:/d:ro …`,
  command in `docs/engine/pitfalls.md`).
- Web pages only through `tools/guides/guide_text.py fetch`, which enforces robots.txt. Never
  wowhead.com, by any route.

**Worktree.**

- Main tree: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-playerbots` on `Custom`.
- Lane worktree: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-icc-wt/<lane>` on `iccr/<lane>`. From
  the main tree: `git worktree add -b iccr/<lane> <wt> Custom`; if the branch exists,
  `git worktree add <wt> iccr/<lane>`; if the worktree exists, reuse it and continue from its state.
- The core's `/build*/` gitignores the path, it stays out of `modules/`, and it sits two levels under
  the core root, so `tools/botobs/raidobs/paths.py` finds the traces.
- Every edit happens in the lane's worktree. Only the merge stage writes the main tree. The guide
  cache `build-icc-wt/_guides/` is outside every worktree and never committed.

**Files a lane may touch.** Its brief's "Owns" list. Anything else only where the brief says so, listed
in the report. Never this file, `src/Ai/Raid/Uld/`, or `src/Ai/Raid/ToC/` (except `r1-foundation`'s
hoist). From wave 3 on, the root strategy, the contexts, the umbrella headers, `HookRows()` in
`src/Ai/Raid/ICC/Util/ICCSeams.cpp` and `docs/raids/icecrown-citadel/README.md` belong to the merge
stage: a boss lane reports what they need.

**Sources.**

- Mechanics: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/src/server/scripts/Northrend/IcecrownCitadel/`
  (`boss_*.cpp`; Svalna, the frost giants, Stinky and Precious and the gauntlets are in
  `icecrown_citadel.cpp`; ids in `icecrown_citadel.h`). Follow a call to its guards before relying on
  its effect.
- Spell ids per difficulty:
  `G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-spell-tweaks/data/dbc-reference/spelldifficulty.reference.csv`
  plus `docker exec ac-database mysql -uroot -ppassword -N -B -e "SELECT … FROM acore_world.spelldifficulty_dbc …"`.
  Spell fields are in `spell.reference.csv` next to the CSV.
- Tactics: the converted guides in `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-icc-wt/_guides/text/`
  (`<slug>.<source>.md`, listed in `INDEX.md`), read word for word. Run the `convert` command from
  `docs/systems/guide-sources.md` first, so pages the user saved since are included. A missing source
  is noted in the report, not waited for. Guide text goes into docs only as paraphrased decisions.
- Coordinates: navprobe per convention 28.
- Traces: `G:/DevStuff/GitHub/azerothcore-wotlk-pb/env/dist/logs/botobs/631_*.ndjson`. Check `hdr.bin`
  before reading one as evidence (`docs/systems/observability.md`, "Is this trace evidence?"). Any
  trace from a build at or after `824468439` shows how the old strategy plays.

**Required reading before designing.** The Conventions sources above;
`docs/raids/icecrown-citadel/README.md` and the lane's boss docs. References by concern:

- interrupt duty: `docs/raids/ulduar/vezax.md`;
- one mover and ranked candidates: `xt002.md`;
- latched stands: `hodir.md`;
- probes and per-instance state: `mimiron.md`;
- taunts and tank swaps: `thorim.md`;
- spread rings: `iron-assembly.md`;
- direct targeting: `ignis.md` and `yogg-saron.md`;
- definitions: `src/Ai/Raid/Uld/Definition/`.

**Checks**, from the worktree root:

- `PB_REPO=<wt> ~/.claude/scripts/pb-check --warnings <changed and new files> src/Ai/Raid/ICC src/Ai/Raid/RaidStrategyContext.h src/Bot/PlayerbotAI.cpp tools/guides`:
  one call runs the syntax check (a `.h` reaches every `.cpp` including it, directly or not), pblint
  scoped to those paths (the whole tree carries known errors), codestyle, the botobs and guides
  tests, and the native tests when a header they include changed. On `INCOMPLETE`, rerun with
  `PB_MAX_FANOUT=all`.
- `python tools/pblint/pblint.py --spell-difficulty src/Ai/Raid/ICC src/Ai/Raid/RaidStrategyContext.h src/Bot/PlayerbotAI.cpp`.
- In a split lane only the finisher runs the checks.

**Commit.** One commit per lane on `iccr/<lane>`, plus one for the reviewer's fixes (message file
`<lane>-review.txt`), staged by explicit path, never `-A`.

- Subject ≤72 characters. Body at most three lines, only a why the diff can't show. Voice per the
  `use-conversational-language` skill. No AI attribution or `Co-Authored-By`.
- Write the message to `G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-icc-wt/_msg/<lane>.txt`, commit
  with `git commit -F` only after that write succeeded, and verify with `git log -1 --format=%B`.
- Code comments follow the no-nonsense-comments rule: no process narration, no lanes, waves or plans.

**Docs.** Durable facts go to `docs/raids/icecrown-citadel/<slug>.md` per `docs/README.md`.

- Each boss doc holds:
  - a **mechanics table**: every ability on 10N/25N/10H/25H with the script's ids and timers, the
    guide's answer, and the bot's answer or the gap;
  - decisions with rationale and source;
  - cheats (replaced, and any survivor with its reason);
  - known gaps;
  - "What a trace answers" (probe and cheat keys, reader flags).
- A generic engine lesson goes into the lane report for the merge stage.
- Fold docs with `compact-docs-writer` (standing approval in `CLAUDE.local.md`).

**Report.** Besides the wave script's fields: `cheatsRemoved`, `cheatsKept` (key, site, reason),
`mechanicsByDifficulty`, `coordinatesChecked`, `guidesRead`, `missingSources`, `bugsFixed`,
`cmakeRerun` (true when the lane added a `.cpp`; the core globs `src/**`).

## Boss lane recipe

Waves 3a-3g run it for every encounter in their brief.

1. **Investigate.**
   - The core script: ids, timers and targeting per difficulty, and spell ids per difficulty.
   - The lane's guides, word for word.
   - The boss doc, the current stem code, and any traces on disk (`postmortem.py`, the boss reader).
2. **Plan.** Write the mechanics table into the boss doc before coding. Write the Task list and
   "Decisions for review" (choice, source, alternative) into the brief.
3. **Rebuild** the encounter to the Conventions.
   - Rewrite rather than patch where the old code breaks them.
   - Remove every icon write and `rti` use the brief lists: a resolver picks the unit and the bot
     attacks it directly.
   - Navprobe every coordinate kept or added.
   - Tank defensives per convention 43.
   - Opening positions live in the definition.
4. **Cheats.** Implement the guide's answer for each cheat and borderline snap in the brief. Apply
   the Cheats decision to any survivor.
5. **Observability.**
   - Keep the contract's slugs and prefixes and `<prefix>.phase`.
   - Probe the new duties and picks inside their helpers.
   - Update the boss reader to show them, with synthetic-pull tests and the `EveryView` check on the
     full fixture.
6. **Doc** per the Docs rule, plus targets for the next 25N pull, then 25H.
7. **Close:** checks, review through the wave script's lenses, fixes, commit.
8. **Review** by a fresh agent (see Waves), before the merge.

## Gap lanes

A gap wave reopens bosses already merged, one `g<n>-<boss>` lane each, and runs the boss lane recipe
on what changes. Each gap ends closed (bots handle it, and the doc says how) or recorded with
the reason it can't close offline:

- a gap only a live pull settles stays, naming the trace key or reader flag that settles it;
- what the Humans decision rules out stays;
- no 631 trace, never built, no `-Werror` pass: those wait for the user's build and pulls.

Every gap lane also:

1. wires tank defensives per convention 43;
2. checks line of sight with navprobe `los` and `ring --los-from` (`docs/engine/pitfalls.md`,
   navprobe bullets) on every stand that must see the boss or a tank, moving a blocked stand and
   re-probing its floor. If navprobe's usage lists no `los`, it records sight as unchecked and goes on.

The shared `src/Ai/Raid/Raid*` files stay as they are: a change one needs goes in the report.

## Waves

| Wave | Lanes, two at a time |
|---|---|
| 1 | `r0a-restructure`, `r0b-guides` (chains into wave 2) |
| 2 | `r1-foundation` |
| 3a | `r2-marrowgar`, `r3-deathwhisper` |
| 3b | `r4-gunship`, `r5-saurfang` |
| 3ab-gaps | `g2-marrowgar`, `g3-deathwhisper`, `g4-gunship`, `g5-saurfang` (see Gap lanes) |
| 3c | `r6-festergut`, `r7-rotface` |
| 3d | `r8-putricide`, `r9-blood-prince-council` |
| 3e | `r10-blood-queen`, `r11-svalna-valithria` |
| 3f | `r12-sindragosa` |
| 3g | `r13-lich-king` |
| 4… | `refine-<boss>`, briefed from the user's pulls |
| 5 | `r14-closeout` |

The user rebuilds once after wave 2, with a CMake re-run, and after any later wave whose report sets
`rebuildNeeded` or `cmakeRerun`. Pulls are never required to start the next wave. Each wave merges
before the next starts.

A lead agent runs each lane: investigate → implement → check → review the change → fix → close and
commit. Only a big lane splits: the lead hands its Task list to up to 3 implementers on disjoint
files, and one finisher integrates, checks, reviews, fixes and commits. Then one fresh reviewer per
lane, which never sees the lead's report, reviews the lane's diff through the same lenses against the
sources, fixes every bug and minor, records what it leaves, re-runs the checks and commits its fixes
on the lane branch; a review it can't get green blocks the lane. Lanes merged before this stage
existed (wave 3a) are reviewed the same way on `iccr/<lane>-review`, a worktree from `Custom`. A merge
stage then lands each finished lane on `Custom`, runs the checks on the merged tree, updates the status table, the ICC
README and the engine docs, and removes merged worktrees and branches. Script:
`G:/DevStuff/GitHub/azerothcore-wotlk-pb/build-toc-wt/icc-rebuild-wave.js`.
