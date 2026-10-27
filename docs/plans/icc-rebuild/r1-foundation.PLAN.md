# r1-foundation — real gates, shared raid helpers, ICC seams

Wave 2, alone, after `r0a-restructure` and `r0b-guides` merged. Rules, decisions and conventions:
[icc-rebuild.PLAN.md](icc-rebuild.PLAN.md). The layout is the one `r0a` wrote (ICC README, "Code
layout"). It may split.

## Scope

1. **Gate mirror.** `Util/ICCEncounterGate.h` mirrors the core's boss ids (`DATA_LORD_MARROWGAR` …
   in `icecrown_citadel.h`, 0-14) with a comment naming that header, as
   `src/Ai/Raid/Uld/UldEncounterGate.h` does for Ulduar.
2. **Real gates.** Swap every definition's pass-through gate:
   - `BossStateGate` with its boss id, 0-13, for the twelve bosses, Svalna (9) and the Sindragosa
     gauntlet (13).
   - `Dogs` and `FrostGiant` have no boss state. Give each a gate built on a generic helper: open
     while the unit, found by entry and the instance, is alive; live while it is in combat with the
     bot's group. Put that helper in `src/Ai/Raid/RaidEncounter.{h,cpp}`, because `r12-sindragosa`
     needs the same for Rimefang and Spinestalker.
   - Every multiplier becomes gated on its definition's live state. This changes play: today all 13
     act in every fight. Log it under Decisions for review.
   - Check each definition's opening steps against when its boss state moves (ICC README, "Boss
     states and trace names"): Festergut and Rotface go `IN_PROGRESS` seconds after the pull,
     Valithria when the adds reach her, Gunship at the battle start. A node that must act before
     that may only depend on "open", not "live". Record per encounter which nodes depend on which.
3. **Side effects out of `IsActive`.**
   - `IccStripExperienced` runs inside 9 triggers. The Putricide and Sindragosa triggers apply the
     `EnableICCBuffs` buffs.
   - Move both into definition `Tick`s: a raid-wide ICC definition for the strip, and the
     Putricide and Sindragosa definitions for their buffs. Behaviour stays the same:
     - the same members get the buffs on the same conditions, recorded through `IccNoteCheat`;
     - neither bots nor humans keep Experienced 71188 or 1% Threat 70115 once the fight that applied
       them is over.
   - Sindragosa's raid cheats in her group-position trigger stay where they are for
     `r12-sindragosa`.
4. **Per-instance state.** `IccInstanceState` moves into `RaidInstanceState<IccState>`
   (`src/Ai/Raid/RaidInstanceState.h`).
   - Keep the traced containers and their keys.
   - Keep the reset semantics: erase 5 s after the last boss state leaves `IN_PROGRESS`, and on map
     destroy.
   - Use `Find` for has-state checks.
5. **Hoist the ToC-only helpers ICC needs** into raid-wide files under `src/Ai/Raid/`, named in the
   `Raid*` style. Point ToC at them in the same commit, with ToC behaviour unchanged:
   - `ToCDirectAttackAction` (`ToC/Action/ToCActions_Shared.h`);
   - `ToC/Util/ToCHelpers_Defensive.{h,cpp}`: the predicted hit, raid covers and personal
     defensives. Turn its map check into a parameter;
   - from `ToC/Util/ToCHelpers_Shared.{h,cpp}`: `IsToCDpsBot`, `GetToCHumanSkull`, `ToCPetOffPick`,
     `IsToCAreaDamage`, `CastToCThroughputHeal`, `ToCRestoreLegacyRti`;
   - the per-encounter burst window (`ToC/Multiplier/ToCMultipliers_Shared.{h,cpp}`), as a hook a
     definition fills, replacing `ToCLiveEncounter` with the caller's live test;
   - the opener hold (`ToC/Util/ToCHelpers_Opener.{h,cpp}`), with the boss list from the caller;
   - the resistance-aura election (`ToC/Util/ToCHelpers_ResistAura.{h,cpp}`), with the school set per
     encounter from the caller.
   - Leave `ToCHelpers_Staging` and `AvoidCreatureClusterAction` in ToC.
   - Another session edits ToC files in the main tree. At the start, read `git -C <main> status`.
     Where a ToC call site sits in a file dirty there, keep the ToC name as a `using` declaration in
     its ToC header, so that file needn't change. List each such alias for `r14-closeout`.
   - ToC parity: the same node, multiplier and definition order, the same names, the ToC readers'
     tests green, pblint over `src/Ai/Raid/ToC` unchanged.
6. **ICC seams** for the boss lanes, empty by default:
   - a direct-attack action on the hoisted base for each definition's resolver;
   - the burst-window hook;
   - opener-hold participation;
   - the resistance-aura school;
   - a place for opening positions.
   Write how a boss lane fills each into the ICC README's "Code layout".
7. **Blood Prince trash trace.**
   - Boss state 14 goes `NOT_STARTED` → `DONE` and never `IN_PROGRESS`, and the trash isn't
     boss-flagged. So add a `RaidObs::MarkPull(631, source, "blood-prince-trash")` hook in
     `Util/ICCScripts.cpp` when a bot enters combat with that trash, with entries from
     `instance_icecrown_citadel.cpp`.
   - The strategy gets no trash code.
   - Update the "Blood Prince trash" section of `docs/raids/icecrown-citadel/blood-prince-council.md`,
     and `tools/botobs/tests/test_blood_prince_trash.py` if it pins the "never opens" fact.
8. **Fold the old plan.** Move the durable content still only in `docs/plans/icc-raidobs/` (decisions
   with rationale, known gaps, carried-over items still true) into `docs/raids/icecrown-citadel/`, then
   delete that directory. Items for later lanes go into their briefs here instead.

## Verified facts

- `RaidEncounterRules::GateOpen` (`src/Ai/Raid/RaidEncounterRules.h`) is closed once the encounter is
  done or while another one is in progress, and open otherwise, so pre-pull steps run. It is also
  open outside an instance and for ids the script lacks.
- `BossStateGateLive` (`RaidEncounter.cpp`) is true only while the state is `IN_PROGRESS`.
- `RaidInstanceState` locks the lookup only: one map updates on one thread at a time, so a reference
  must not outlive the tick. `IccState()` follows the same model today.
- ToC coupling points: `ToCHelpers_Defensive.cpp` has one map check; `ToCHelpers_Shared.cpp` has one;
  the burst window, opener and resist aura read `ToCLiveEncounter`; the opener also reads a hard-coded
  `ToCNpcs` boss list, and the resist aura `ToCStagingBoss`.
- Another session reworks ToC's Anub'arak in the main tree (last commit `fdb3a950f`); its files go
  dirty and clean again. Check `git status` at the start and again before the commit.
- The 13 multipliers and what each vetoes are listed in the boss briefs. Gating them is this lane's
  change; their content is the boss lanes'.

## Owns

Everything under `src/Ai/Raid/ICC/`. New raid-wide files under `src/Ai/Raid/` and the generic gate
helper in `RaidEncounter.{h,cpp}`. The ToC files the hoist touches, within scope item 5.
`tools/nativetest/` tests for the hoisted code if any exist. `tools/botobs/tests/test_blood_prince_trash.py`,
`docs/raids/icecrown-citadel/`, `docs/raids/trial-of-the-crusader/README.md` (code layout lines for the
hoist), `docs/plans/icc-raidobs/` (deleted), and this brief.

## Task list

Split: three implementer groups run in parallel in this worktree on disjoint files, then one finisher.
Implementers run no checks, stage nothing and commit nothing; each reports what it changed, its new
API and the facts the finisher needs. Scope numbers refer to the Scope list above. Baselines at
`206bacaca`: pblint `--warnings` over `src/Ai/Raid/ToC` clean, over the ICC check set 0 errors and 22
warnings (all `strategy-never-added`), `--spell-difficulty` over ICC and ToC clean, botobs tests 1140
OK, guides tests 25 OK. No 631 trace is on disk; no guide applies to this lane.

### Group A `hoist` (scope 5)

Files: create `src/Ai/Raid/RaidDirectTargeting.{h,cpp}`, `RaidCombatHelpers.{h,cpp}`,
`RaidDefensive.{h,cpp}`, `RaidBurstWindow.{h,cpp}`, `RaidOpener.{h,cpp}`, `RaidResistAura.{h,cpp}`
(all under `src/Ai/Raid/`); edit or delete files under `src/Ai/Raid/ToC/`; edit
`docs/raids/trial-of-the-crusader/README.md` ("Code layout" lines only).

1. Move, keeping comments and behaviour:
   - `RaidDirectTargeting`: `ToCDirectAttackAction` → `RaidDirectAttackAction`, `GetToCHumanSkull` →
     `GetRaidHumanSkull`, `ToCPetOffPick` → `RaidPetOffPick`, `ToCRestoreLegacyRti` →
     `RaidRestoreLegacyRti(botAI, mapId)`.
   - `RaidCombatHelpers`: `IsToCDpsBot` → `IsRaidDpsBot`, `IsToCAreaDamage` → `IsRaidAreaDamage`,
     `CastToCThroughputHeal` → `CastRaidThroughputHeal`.
   - `RaidDefensive`: all of `ToC/Util/ToCHelpers_Defensive.{h,cpp}` under `Raid` names
     (`RaidPredictedHit`, `RaidCover`, `RaidPersonalPick`, `GetRaidCover`, …). The map 649 check and
     the keys `toc.cover`, `toc.personal` become a scope the caller passes (map id and both keys).
   - `RaidBurstWindow`: `ToCBurstWindow` and `ToCBurstWindowMultiplier` → `RaidBurstWindow` and
     `RaidBurstWindowMultiplier(botAI, name, window)`, `window` a `RaidBurstWindow (*)(PlayerbotAI*)`.
     ToC passes its `ToCLiveEncounter` switch under the name `toc burst window`.
   - `RaidOpener`: `ToCHelpers_Opener` plus the opener trigger, action and multiplier classes. Map id,
     note key and the boss entries of the caller's live encounter come from a caller config; the
     classes take their names from the constructor, so ToC registers them under today's names.
   - `RaidResistAura`: `ToCHelpers_ResistAura` plus its trigger and action classes, same way: map id,
     note key, the wanted schools given the previous set, and whether fire wants Aura Mastery come
     from the caller.
   - Stay in ToC: `ToCHelpers_Staging`, `AvoidCreatureClusterAction`, `ToCMainTankHoldAction`,
     `GetNearestCreatureByEntry`, `GetCreatureClusterCenter`.
   - A per-instance store whose traced container carries a probe key builds it with the caller's
     key; instance ids are unique across maps.
2. Point every ToC call site at the new names. First run `git -C
   G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-playerbots status --short src/Ai/Raid/ToC`:
   leave any file dirty there unedited, keep its ToC name as a `using` declaration in its ToC
   header, and list each alias in the report. Clean at `206bacaca`, so expect none.
3. ToC parity: the same node, multiplier and definition order; the same trigger, action, multiplier
   and rule names; the same probe keys (`toc.cover`, `toc.personal`, `toc.opener`, `toc.aura`) and
   values.
4. Report one line per public symbol of each new file, and how a raid other than ToC supplies each
   config.

### Group B `icc-core` (scope 1-4, 7)

Files: `src/Ai/Raid/RaidEncounter.{h,cpp}`, everything under `src/Ai/Raid/ICC/`,
`tools/botobs/tests/test_blood_prince_trash.py`.

1. Framework, `RaidEncounter`; Ulduar, ToC, EoE, OS, Onyxia and Naxx behaviour unchanged:
   - A unit gate over creature entries given as template arguments (`UnitGate<A, B>`): open while a
     unit of one entry is alive in the bot's instance, live while that unit is in combat and its
     victim or the bot is in the bot's group. Cache per gate pass as `BossStateGateOpen` does, plus a
     per-instance guid cache (`RaidInstanceState`) refreshed by a throttled scan: no map-wide scan per
     check. A unit in an unloaded grid isn't found, so the gate stays closed.
   - `EncounterBuilder::LiveMultiplier<M>(families, args...)`: as `Multiplier`, but it sees actions
     only while the gate is live. Rules and `Multiplier` keep their open gating.
   - Public `EncounterDefinition::Open(botAI)` and `Live(botAI)`, for the finisher's seams.
2. Gates: each boss definition takes `BossStateGate` with its `IccEncounterId` (0-12, Svalna 9);
   `Dogs` a unit gate on Stinky 37025 and Precious 37217, `FrostGiant` on 38490 and 38494, both still
   `ICC_BOSS_NONE`. No gauntlet definition exists: id 13 is `r12-sindragosa`'s. Rename
   `IccPassThroughGate` to `IccRaidWideGate` (open, never live), for raid-wide definitions with
   `ICC_BOSS_NONE` and no trace name.
3. Multipliers: each of the 13 becomes a `LiveMultiplier` unless it must act while its definition is
   open but not yet live. Check each against the ICC README's "Boss states and trace names" table and
   the core script (Festergut and Rotface go live seconds after the pull, Valithria when the adds
   reach her, Gunship at the battle start). Report per encounter which nodes and multipliers act
   before live.
4. Side effects out of `IsActive`:
   - New `Definition/ICCDefinition_HeroicBuffs.cpp` (`IccHeroicBuffsDefinition()`, `IccRaidWideGate`,
     no rows), listed last in `IccEncounterDefinitions()`. Its tick strips Experienced 71188 and 1%
     Threat 70115 from the bot, and from every human in its instance behind a per-instance throttle,
     whenever none of Deathwhisper (1), Putricide (6), Sindragosa (11) and the Lich King (12) is
     `IN_PROGRESS`. Cheap checks first: those boss states, the bot's own auras, then the sweep. Remove
     `IccStripExperienced` from the 9 triggers that call it.
   - The Putricide and Sindragosa definitions get a tick holding the `EnableICCBuffs` block now in
     `IccPutricideMalleableGooTrigger::IsActive` and `IccSindragosaGroupPositionTrigger::IsActive`:
     the same members, conditions and `IccNoteCheat` keys, behind the live check before any lookup.
     Sindragosa's raid cheats and Nitro Boosts stay in that trigger for `r12-sindragosa`.
   - `IccMarkPull` moves from the `Dogs` and `FrostGiant` triggers into their definitions' ticks,
     while live.
5. Per-instance state: `IccInstanceState` lives in a `RaidInstanceState<IccInstanceState>`.
   `IccState(id)` uses `For`; `IccBossStateResetScript` stamps through `For` only while a boss is
   `IN_PROGRESS` and otherwise reads through `Find`, keeping the 5 s grace. Delete
   `IccMapCleanupScript`: `RaidInstanceStateMapScript` drops every store on `OnDestroyMap`. Traced
   containers and their keys stay.
6. Blood Prince trash: in `Util/ICCScripts.cpp`, a `UnitScript` on `UNITHOOK_ON_UNIT_ENTER_COMBAT`
   calls `RaidObs::MarkPull(map, creature, "blood-prince-trash")` when a creature on map 631 with
   entry 37571, 37664, 37595 or 37663 engages a player. No strategy code. Change
   `test_blood_prince_trash.py` only if a test pins that no trace opens.
7. Report the facts the ICC README needs: gates, the raid-wide definition, per-instance state, the
   trash hook, and the open/live list from task 3.

### Group C `docs-fold` (scope 8)

Files: `docs/plans/icc-raidobs/` (delete from disk, unstaged), `docs/raids/icecrown-citadel/*.md`,
`docs/plans/icc-rebuild/r2-marrowgar.PLAN.md` to `r14-closeout.PLAN.md`. Invoke the
`compact-docs-writer` skill for these edits (standing approval in `CLAUDE.local.md`).

1. Read every file in `docs/plans/icc-raidobs/`. Move durable content not yet in
   `docs/raids/icecrown-citadel/` (decisions with rationale, known gaps, carried-over items still true
   of the current code) into the matching boss doc or the README. Drop process narration and whatever
   `icc-rebuild.PLAN.md`'s Decisions supersede. An item a later lane must act on goes into that
   lane's brief (Scope or Carried over), not a boss doc.
2. Point the boss docs' `Code:` lines at the per-stem files `r0a-restructure` created.
3. `blood-prince-council.md`, "Blood Prince trash": the trace now opens through the hook in task B6
   (any spawn of the four entries engaging a player, 18 spawns on 631); the `DONE` still closes it as
   `kill`.
4. Replace noobtoboss in the `r3`, `r4`, `r11`-`r13` briefs' source lists: its robots.txt refuses
   agents.
5. In the README, leave "Code layout and registration", "Per-instance state" and the strip lines of
   "Cheats" to the finisher.

### Finisher

Reads the three reports and the diff, then:

1. ICC seams (scope 6), empty by default, with the per-definition hooks in one ICC place:
   - direct attack: one shared ICC base on `RaidDirectAttackAction` taking the resolver, its
     "resolver returns a unit" trigger, and the `OwnTargeting` rule a boss lane adds with them; no
     empty per-definition nodes;
   - burst: `RaidBurstWindowMultiplier` named `icc burst window` in the root, last, asking the live
     definition's hook, allow-all by default;
   - opener: the hoisted opener for ICC under `icc.opener`, its bosses from the live definition,
     noting nothing while no definition lists any;
   - resistance aura: the election for ICC under `icc.aura`, its schools from the live definition;
   - a defensive scope for map 631 with `icc.cover` and `icc.personal`;
   - opening positions: where a boss lane puts them in its definition.
2. ICC README: "Code layout" (each seam and how a boss lane fills it), the gates replacing
   "Pass-through gate" and "Triggers act inside `IsActive`", "Per-instance state", the strip lines
   in "Cheats", and B's open/live list.
3. This brief: Decisions for review, Carried over, Known gaps, any alias from A.
4. Checks, the review through the wave script's three lenses, fixes, docs fold, commit.

## Decisions for review

| Choice | Source | Alternative |
|---|---|---|
| Split into `hoist`, `icc-core`, `docs-fold` plus a finisher | lane size: ~1500 ToC lines moved over ~30 files, 15 definitions, 16 state users, a 1300-line plan to fold | one agent |
| Multipliers go live-gated through a new `LiveMultiplier`; one that must act before live stays open-gated and is listed | scope 2 | gate the rules too (ICC has none yet) |
| The unit gate is a template over entries with a per-instance guid cache | scope 2, convention 33 | `GetCreatureListWithEntryInGrid` around the bot, which misses a unit out of range |
| No gauntlet definition in this lane | none exists; `r12-sindragosa` owns the gauntlet | an empty definition on `BossStateGate(13)` now |
| The strip runs whenever none of Deathwhisper, Putricide, Sindragosa, the Lich King is `IN_PROGRESS` | scope 3, "once the fight that applied them is over" | strip only in the eight fights that did it, so the buffs ride through Gunship and trash |
| The trash hook keys on the four entries, any spawn, any player victim | scope 7 ("entries") | the four spawn ids 201479, 201482, 201646, 201659 behind state 14; bot victims only, which misses a human tank's pull |
| `IccMapCleanupScript` goes | `RaidInstanceStateMapScript` evicts every store | keep both |
| `IccMarkPull` for Dogs and FrostGiant moves into their ticks | convention 4 | leave it in `IsActive` |
| Hoisted helpers take caller configs; ToC keeps its names and keys; ICC's are `icc.cover`, `icc.personal`, `icc.opener`, `icc.aura` | scope 5, naming contract (`icc.` is raid-wide) | a boss prefix, which a raid-wide helper has none of |
| The direct-attack seam is one shared base, not 15 empty resolver nodes | conventions 5-9 | per-definition skeletons that never fire |
| `RaidRestoreLegacyRti` is not wired for ICC yet | ICC code still writes `rti`; wire it once no ICC code does (`r14-closeout` at the latest) | wire it now, and the old code rewrites `rti` anyway |
| No `using` aliases | main tree clean at `206bacaca` | aliases everywhere for a smaller ToC diff |
| Gates change play: every other encounter's nodes close while one is in progress, and every multiplier but Festergut's and Rotface's acts only while its fight is live | scope 2 | pass-through: all 13 multipliers and 51 nodes in every fight |
| Festergut's and Rotface's multipliers stay open-gated | their state moves when Putricide's 15 yd/s charge reaches his balcony, ~102 and ~104 yd (~7 s) into the pull; their bodies key on `"find target"` | `LiveMultiplier`, which drops their pull-window vetoes |
| Unit gate live: the unit's victim is in the bot's group (a pet or totem counts as its owner) or the bot is in combat with it | scope 2 ("its victim or the bot") | victim only |
| The unit gate's cache scans the map object store, at most once a second per instance and at once for a new entry | brief ("throttled scan"); catches summons, misses unloaded grids | the spawn-id store (DB spawns only); a grid search around the bot (misses a unit out of range) |
| The strip sweeps every human on the instance map | scope 3 ("every human in its instance") | group members only, as the old code did |
| The trash hook needs a player victim | scope 7 ("engages a player"), like RaidObs's engage hook | count a pet's owner, which also catches a pet pull |
| `ActiveGooPositions` reads through `Find` | convention 26 | `For` |
| noobtoboss is replaced by instant-carry's raid overview (`icecrown-citadel.ic`) in r3, r11-r13 and dropped in r4, which already listed it; r3's Dominate Mind and r13's Shadow Trap facts now cite the core script | its robots.txt refuses agents | another site; none of the rest answers scripts |
| Boss docs' `Code:` lines name the stem pattern `src/Ai/Raid/ICC/*/ICC*_<Stem>.*` | r0a's layout | list every file |
| README cheat classes: `remove` dropped (no row has it), `gap` restated to the Cheats decision | overview Cheats | keep the old classes |
| r0a's dead code goes into r3's and r7's Carried over (the giant's WiP comment and TODO, `spellEntriesFlood`, the commented-out `IccRotfaceGroupPositionMultiplier`) | still at HEAD | leave it for the lanes to find |
| One hooks table (`HookRows()`, `Util/ICCSeams.cpp`) answers burst, opener and aura from the first live row | finisher brief ("the per-definition hooks in one ICC place") | a hook set in each definition file, which keeps paired lanes off one shared file |
| Hooks answer only while live | finisher brief ("the live definition") | live or open, which can't tell which open encounter is being pulled; Festergut, Rotface and Valithria lose the pre-live seconds (Known gaps) |
| The ICC opener swing and aura triggers return false, noting nothing, while the live row lacks that hook; ToC's classes are unchanged | finisher brief; no key that can never emit | a flag in the raid-wide configs, which touches ToC |
| One `RaidWide` definition holds the opener and aura rows, listed after the Lich King; its multipliers follow Gunship, then `icc burst window` | integration note 2 | two definitions as in ToC |
| Their priorities are half steps in ToC's relative order: swing `ACTION_RAID + 2.5`, aura `+ 1.5`, redirect `+ 0.5` | convention 20: ToC's `+ 2`, `+ 1`, `+ 1` tie each other and ICC's integer rows | ToC's values, ties settled by insertion order |
| The direct-targeting bases take the resolver at construction; `IccResolverPicks<&R>` is the rule's predicate | conventions 5-9; the naming test parses `Name` classes | a template class per resolver; a builder helper the naming test can't parse |
| Opening positions are documented, not coded | no empty per-definition nodes; Pre-pull decision | an empty `Mover` row per definition |
| `ToCHelpers_Anubarak.cpp` and `ToCTriggers_Anubarak.cpp` stay at `Custom`'s text; `ToCHelpers_Shared.h` aliases `IsToCDpsBot`, `GetToCHumanSkull` and `ToCPetOffPick` | scope 5: both went dirty in the main tree after A's check | edit them and have the merge refused |
| `probes.py` gets no pattern for keys passed through a config | an empty ICC seam would read silent on every ICC pull | add it now, so ToC's banners list `toc.cover`, `toc.personal`, `toc.opener`, `toc.aura` again |
| Outside Owns: `test_icc_naming.py`'s `RAID_WIDE_TRIGGERS`, the `blood_prince_trash.py` docstring, a `lord_jaraxxus.py` comment, the ToC README's symbol names outside "Code layout" | they describe this lane's change | leave them stale |

## Carried over

- **r14-closeout**: point `ToCHelpers_Anubarak.cpp` at `IsRaidDpsBot` and `GetRaidHumanSkull`, and `ToCTriggers_Anubarak.cpp` at `RaidPetOffPick`, then drop the three aliases in `ToC/Util/ToCHelpers_Shared.h`, once the Anub'arak session has both files clean.
- **r14-closeout**: ToC boss docs name pre-hoist symbols: `anubarak.md` 432, 491, 805; `faction-champions.md` 276, 297, 299; `lord-jaraxxus.md` 170, 171, 205, 280; `northrend-beasts.md` 64, 449; `twin-valkyr.md` 173, 193, 195, 196.
- **r14-closeout**: `tools/botobs/raidobs/probes.py` `DECLARATIONS` misses a key passed through a config initializer (`RaidDefensiveScope`, `RaidOpenerConfig`, `RaidResistAuraConfig`), so `toc.cover`, `toc.personal`, `toc.opener`, `toc.aura` and the `icc.` four are missing from `declared_keys()`. Add the pattern once an ICC seam emits.
- **Merge stage**: `docs/raids/README.md` (~line 365, "Not migrated, on purpose: … ICC") is stale: ICC's state is in `RaidInstanceState` now.
- **Merge stage**: `HookRows()` in `Util/ICCSeams.cpp` is registration every boss lane needs, like the root strategy; the overview's Lane rules should list it with the merge stage's files from wave 3.
- **r6-festergut, r7-rotface, r11-svalna-valithria**: an opener or aura hook sees nothing before the state moves (Festergut and Rotface ~7 s into the pull, Valithria once the first archmage pulls the combat trigger).

## Known gaps

- Never built. CMake re-run needed: 11 `.cpp` added (six `src/Ai/Raid/Raid*.cpp`, `ICCDefinition_HeroicBuffs`, `ICCDefinition_RaidWide`, `ICCSeams`, `ICCTriggers_RaidWide`, `ICCActions_RaidWide`), four ToC `.cpp` deleted.
- Hooks answer only while live (above).
- A Stinky, Precious or giant in an unloaded grid leaves its gate closed.
- A Blood Prince trash pull a pet opens is traced only once one of the four engages a player.
- The direct-targeting, burst, opener, aura and defensive seams have no ICC user yet: the boss lanes fill them.

## Blocked
