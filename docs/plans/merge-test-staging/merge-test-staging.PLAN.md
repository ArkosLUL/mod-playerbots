# Merge `test-staging` into `Custom` — conflict analysis and resolution plan

## Context

`Custom` is a heavily diverged fork of mod-playerbots: **166 commits** ahead of merge-base
`8f812e35e`, ~399 files changed, 141 new files (Naxx/Uld/ToC/SWP/Aq40 raid strategies, smart-loot
port, autogear/gemming/tinker work). Upstream `test-staging` is **25 commits** ahead of the same
base, ~815 files changed.

`git merge test-staging` produces **38 conflicted files**. This plan classifies every one of them,
flags the auto-merged changes that are semantically risky, and gives the resolution.

Verification below was done read-only via `git merge-tree --write-tree Custom test-staging`
(result tree `d264ea5e5a66767ff5a8cb70a8a172f3a3dc3770`), so all claims about the merged result are
checked against the actual merge output, not guessed.

**Headline finding:** conflict volume is large but shallow. Roughly 30 of 38 conflicts are pure
`#include` block collisions caused by upstream PR #2576 ("standardize include order, make headers
self-contained") and #2551 ("include cleanup"). Only **five** conflicts carry real semantics.

## What is coming in (25 commits)

Notable, ranked by risk to our tree:

| PR | What | Risk to us |
|----|------|-----------|
| #2576 + #2551 | Include-order standardization, self-contained headers (946 include lines removed, 1041 added) | **High** — our 141 new files may have relied on transitive includes that are now gone. Compile-time only. |
| #2592 | `IsRealPlayer` semantics flipped; `HasRealPlayerMaster` / `HasActivePlayerMaster` **deleted**, replaced by free `IsRealPlayer(Player*)`, `IsSelfBot(Player*)`, `HasGameClientMaster()` | **Was high, verified clear** — see below |
| #2605, #2490 | Equip persistence on downlevel; expanded autogear command | Medium — overlaps our gemming/tinker work in `PlayerbotFactory.cpp` |
| #2496 | `LootRollAction::Execute` perf | Medium — we already ported an equivalent; direct conflict |
| #2558 | mod-player-bot-level-brackets + mod-player-bot-reset integration (new `RandomBotLevelMgr`, +317 conf lines) | Low — purely additive, we never touched `RandomPlayerbotMgr` |
| #2628, #2634, #2570 | Karazhan/Nightbane rewrite, Sethekk Halls, Mechanar Sepethrea kite | Low — zero overlap, we touch no Kara/Sethekk/Mechanar files |
| #2633 | `currentBots` linked list → hash set | None — we never touched `RandomPlayerbotMgr.*` |
| #2503, #2598, #2586, #2606, #2617, #2559 | Arena captain, double-free on disable, id1 fixes, loot GO scoping, stats interval | None |

### #2592 dropped-API check — already verified safe

Both dropped methods were grepped in the **merged tree**: zero hits for
`HasRealPlayerMaster|HasActivePlayerMaster`, zero hits for `->IsRealPlayer(`. Upstream's PR touched
every file that used them except [BossAuraActions.cpp](src/Ai/Base/Actions/BossAuraActions.cpp),
which is a conflicted file anyway, and git auto-applied their rewrites into our diverged copies
cleanly. Example: our `MasterLootRollAction::isUseful()` becomes
`return !IsRealPlayer(botAI->GetMaster());` without conflict.

**Still watch for:** `IsRealPlayer` changed *meaning* (was "is a selfbot", now "is a plain player
with no bot AI"). Any of our new code calling the free `IsRealPlayer` should be re-read — but the
grep above shows we have no such call sites, so this is a no-op for us.

### Auto-merged, verified clean

- `conf/playerbots.conf.dist` (333 their lines vs 276 ours) — merged with **no conflict markers,
  no duplicate keys**, 923 keys total.
- `src/Ai/Raid/Uld/Util/UldScripts.h` — upstream added an `enum UlduarNPCs`; verified **no
  duplicate definition** of `NPC_SALVAGED_DEMOLISHER_TURRET` etc. anywhere in the merged tree.
- No `CMakeLists.txt` in this module (AzerothCore globs sources), so our Uld file split needs no
  build-file edits.
- No leftover references to the three deleted `src/Ai/Raid/Uld/Uld{Actions,Triggers}.*` paths.

## Behavior changes after the merge

What actually changes at runtime on `Custom` once this merge lands and the resolutions below are
applied. Grouped by whether it needs config to take effect.

### Changes bots' in-game behavior, on by default

1. **Karazhan / Nightbane rewritten (#2628).** Nightbane no longer uses hardcoded coordinates.
   The tank now positions itself between Nightbane and the balcony so the boss faces off the
   balcony, instead of dragging it through two fixed points. Ranged bots read Charred Earth from
   dynamic objects and stack in a coordinated way with min/max distance from the boss, replacing
   the old fixed 3-position cycle. **RTI marking is removed except selective Skull use** — bots
   stop clobbering the raid's existing RTI assignments in Kara, and no longer need marks placed to
   target correctly. Minor tweaks also land for Attumen, Maiden of Virtue, Moroes, Netherspite,
   Prince Malchezaar, Shade of Aran, The Curator, Big Bad Wolf, Terestian Illhoof.
   Applies on map 532 whenever the `karazhan` strategy is active.
2. **Sethekk Halls gets a strategy (#2634)**, normal and heroic, auto-applied on map 556 as
   `tbc-seth`. Bots skull-mark Charming Totems summoned by Time-Lost Controllers; shaman drop
   Tremor Totem against Sethekk Prophets (temporary override, no strategy swap, your Earth Totem
   strategy stays); Darkweaver Syth's elemental adds get marked Frost → Shadow → Arcane → Fire.
   Talon King Ikiss previously needed manual bot movement — no longer.
3. **Mechanar: Sepethrea kiting (#2570)**, auto-applied on map 554 as `tbc-mech`
   (kite flame, avoid flame, avoid trail, focus boss).
4. **Bots loot more than before (#2606).** The disallowed-gameobject config list was being matched
   against *every* loot object's entry. It now only applies when the loot object really is a
   gameobject, so creature/item loot whose entry happened to collide with a blacklisted GO entry is
   no longer skipped.
5. **Gear is re-rolled when a bot levels down (#2605).** Previously `equipAndSpecPersistenceLevel`
   kept the old gear; now a level *decrease* also forces a re-roll, so downleveled bots stop
   walking around in gear they can't use.
6. **Arena team joining fixed (#2503)** — captain selection is corrected and bots are forced into
   only one team, which stops bots pulling each other out of groups when queueing.
7. **`.playerbots disable` no longer double-frees (#2598)** — the redundant `delete` of the bot's
   `travel target` is gone. This was a crash, not a behavior tweak.
8. **`rpg status` can now set state (#2372)**, not just report it. Restricted to the bot's master
   or a GM; the bot whispers a localized confirmation. Ships new
   `ai_playerbot_rpg_status_texts` SQL — **this needs applying to the world DB.**
9. **New guild-bank / bank / trade keywords (#2574, #2575):** `cloth`, `leather`,
   `metal`/`stone`/`ore`, `meat`, `herb`, `elemental`, `enchanting`/`disenchants`, plus a new
   `materials` aggregate and `recipe all`. Gray junk is skipped for the trade-goods categories.
   `recipe` and bot auto-learning are unchanged.
10. **Korean `ai_playerbot_texts` localization (#2547)** — more SQL to apply.
11. **`PrintStats` role counts shift slightly (#2559).** It now uses the cached activity flag and
    calls `IsHeal(bot, false)` / `IsTank(bot, false)` instead of the recomputing `true` variant.
    Log numbers only; no bot behavior change.
12. **Stats log interval is configurable (#2617)** — `AiPlayerbot.RandomBotPrintStatsInterval`,
    default 300s (unchanged), `0` disables the log.

### Inert unless you turn it on

13. **Level brackets + bot level reset (#2558)** — a whole new `RandomBotLevelMgr` (1154 lines) and
    317 lines of config. Both master switches default off: `AiPlayerbot.LevelBrackets.Enabled = 0`,
    `AiPlayerbot.ResetBotLevel.Enabled = 0`. Nothing changes until you enable them.

### No behavior change at all

14. **#2592 is a pure rename** — verified line by line against the base:
    `HasRealPlayerMaster()` ≡ `HasGameClientMaster()`,
    `HasActivePlayerMaster()` ≡ free `IsRealPlayer(master)`,
    old member `IsRealPlayer()` ≡ free `IsSelfBot(player)`. Same truth values, new names. The only
    cost is compile surface, and the merged tree already has zero dangling references.
15. **#2633** `currentBots` linked list → hash set: pure lookup perf in `RandomPlayerbotMgr`.
16. **#2496** `LootRollAction::Execute` perf: we already ship the equivalent.
17. **#2576, #2551, #2640, #2641, #2569, #2586** — include order, self-contained headers,
    `virtual` → `override`, namespace unindenting, warning cleanup (`class Position` → `struct
    Position`, constexpr action weights), and an SQL `id1` fix. Build-level only.

### Behavior we deliberately keep by resolving in our favour

- **Loot rolling stays on smart-loot.** Upstream's `LootRollAction::CalculateRollVote` (a flat
  `ItemUsage` → `RollVote` switch) is dropped in favour of our
  `CalculateLootRollVote(bot, proto, randomProperty, usage, group)`, which is random-property aware.
  Upstream's ML/FFA → `PASS` rule is already in our version, so nothing is lost.
- **All four Naxx bosses keep our implementations** (Noth, Faerlina, Maexxna, Gothik). Upstream's
  side of those conflicts is literally `// Reserved for <boss>-specific actions.` — they never had
  the logic.
- **Ulduar keeps our per-boss file split.** Upstream's only change to the three monolithic files we
  deleted was include reordering.

### One thing to check after the build is green

`.playerbots autogear` was refactored into `PlayerbotFactory::AutoGear(...)` (#2490) and now takes
an `applyFinishers` flag. Our gemming/tinker work lives inside `ApplyEnchantAndGemsNew()` and the
new `ApplyPrismaticSocket()`, which `AutoGear` **does** call — but only when `applyFinishers` is
true. Confirm no caller passes `applyFinishers = false` on a path where you expect gems and tinkers,
or `.playerbots autogear` will hand back ungemmed gear while levelup still gems correctly.

## Conflict resolution table

### Group A — delete/modify, 3 files → `git rm`

`src/Ai/Raid/Uld/UldActions.cpp`, `UldTriggers.cpp`, `UldTriggers.h`

We split these into `src/Ai/Raid/Uld/Action/UldActions_*.{h,cpp}` and
`src/Ai/Raid/Uld/Trigger/UldTriggers_*.{h,cpp}`. Upstream's only change to them was include
reordering — **nothing of substance is lost**. Resolution: `git rm` all three.

### Group B — take OURS wholesale, 5 files

Upstream only did include hygiene here; we rewrote the files entirely.

- `src/Ai/Raid/Naxx/Action/NaxxActions_Noth.cpp` (390 conflicted lines)
- `.../NaxxActions_Faerlina.cpp` (111)
- `.../NaxxActions_Maexxna.cpp` (88)
- `.../NaxxActions_Gothik.cpp` (57)
- `src/Ai/Raid/Naxx/NaxxMultipliers.h` (4 hunks) — their side is the *commented-out* Heigan/Gothik
  multiplier stubs plus `virtual`→`override` style edits; we implemented those multipliers for
  real. Take ours, but **do apply their `virtual float GetValue(Action*)` → `float GetValue(Action*)
  override`** style change to our implementations (PR #2640) so we don't regress against a future
  `-Werror` promotion (#2557).

Cross-check: `git diff --stat 8f812e35e test-staging -- src/Ai/Raid/Naxx/` is 23 files / 23
insertions / 46 deletions, all includes. Ours is 27 files / 6767 insertions.

### Group C — union of both include lists, 24 files

Mechanical. Keep every include from both sides, then apply upstream's convention: **quoted form,
alphabetically sorted, `<system>` headers in their own block**. Our angle-bracket project includes
(`#include <HunterBuffStrategies.h>`) must become quoted.

Representative paths (same treatment for all):
`src/Ai/Base/Actions/BossAuraActions.cpp`, `src/Ai/Base/Trigger/{BossAuraTriggers,GenericTriggers}.cpp`,
`src/Ai/Base/Trigger/GenericTriggers.h`, `src/Ai/Base/Trigger/HealthTriggers.cpp`,
`src/Ai/Base/Value/ItemUsageValue.cpp`, `src/Ai/Class/{Rogue/RogueTriggers,Warlock/WarlockTriggers}.cpp`,
`src/Ai/Raid/Naxx/Action/NaxxActions_{Anubrekhan,Heigan,Kelthuzad,Loatheb,Sapphiron,Thaddius}.cpp`,
`src/Ai/Raid/Naxx/{NaxxBossHelper.h,NaxxMultipliers.cpp}`,
`src/Ai/Raid/OS/{OSMultipliers,OSTriggers}.cpp`,
`src/Ai/Raid/Uld/Util/UldBossHelper.{h,cpp}`,
`src/Bot/{PlayerbotAI.h,Factory/PlayerbotFactory.cpp}`, `src/Mgr/Item/StatsWeightCalculator.cpp`,
`src/PlayerbotAIConfig.cpp` (union `<algorithm> <iostream>` + `ProgressionMgr.h` + `Playerbots.h`).

### Group D — union but content-bearing, 4 files

**`src/Ai/Base/TriggerContext.h`** — ours adds `RitualOfSoulsActions.h`, theirs adds
`PvpTriggers.h` + `RangeTriggers.h`. Union; these back real trigger registrations on both sides.

**`src/Ai/Raid/RaidStrategyContext.h`** (3 hunks) — ours registers `RaidAq40Strategy.h`,
`MCStrategy.h`, `HyjalStrategy.h`, `SWPStrategy.h`, `ToCStrategy.h`; theirs adds `BTStrategy.h`,
`UldStrategy.h`, `VoAStrategy.h`, `Strategy.h`. **Union — dropping either side silently unregisters
raid strategies.** Keep our path-prefixed includes as-is (they compile today on `Custom`).

**`src/Bot/Engine/BuildSharedActionContexts.cpp`** and **`BuildSharedTriggerContexts.cpp`** (2 hunks
each) — same story: ours carries `Ai/Raid/Aq40/*`, `MCActionContext.h`, `HyjalActionContext.h`,
`SWPActionContext.h`, `Ai/Raid/ToC/*`, `Ai/Dungeon/TOC/*`; theirs carries
`WorldPacketActionContext.h`, `TbcDungeonActionContext.h`, `WotlkDungeonActionContext.h`, etc.
**Union.** These are the registration sites for the new Sethekk/Mechanar/Karazhan work coming in —
losing their lines means their new strategies never register.

### Group E — real semantics, 2 files

**`src/Ai/Base/Actions/LootRollAction.cpp`** (4 hunks) — **take OURS**.

We already implement upstream #2496's improvement (`voted` accumulator instead of early
`return true`, ML/FFA → `PASS`) and went further: smart-loot routes through the free
`CalculateLootRollVote(bot, proto, randomProperty, usage, group)` and
`ItemUsageValue::BuildItemUsageParam(...)`, and we deleted the `LootRollAction::CalculateRollVote`
member. Their conflict hunk re-adds that member definition — **drop it**. Verified: the merged
`LootRollAction.h` already has no `CalculateRollVote` declaration, so keeping their definition would
not even compile. Their `MasterLootRollAction::isUseful()` rewrite auto-merged already, no action.

Also keep our `std::vector<Roll*> const& rolls = group->GetRolls();` + `for (Roll* const roll : rolls)`
over their by-value copy + `Roll*&` — ours avoids the vector copy and is strictly better.

**`src/Bot/PlayerbotAI.cpp`** — `GetInstanceStrategies()` list. **Union, alphabetically sorted.**
Ours adds `"aq40"`, `"sunwell"`, `"trialofthecrusader"`; theirs adds `"tbc-mech"`, `"tbc-seth"`.
Final list must contain all five. Dropping ours disables our raids; dropping theirs disables the
incoming Mechanar/Sethekk strategies.

## Execution

Approved: scratch branch, agent runs the merge, stops **before** committing.

1. `git checkout -b merge/test-staging-20260808 Custom`
2. `git merge --no-commit --no-ff test-staging` (expect 38 conflicts)
3. `git rm` the three Group A Uld files
4. `git checkout --ours` the five Group B files, then hand-apply the `override` style change to
   `NaxxMultipliers.h`
5. Hand-resolve Groups C/D/E per the table above
6. `git grep -n "<<<<<<<\|>>>>>>>" -- src conf` → must be empty
7. `git add -A`, leave **staged and uncommitted** for review

The commit itself is a separate explicit go-ahead — this plan does not authorize it.

## Verification

Static (agent, before hand-off):

- `git grep -nE "HasRealPlayerMaster|HasActivePlayerMaster" -- src` → must be empty
- `git grep -nE "(botAI|ai)\s*->\s*IsRealPlayer" -- src` → must be empty (it is now a free function)
- `grep -oE "^[A-Za-z][A-Za-z0-9._]*\s*=" conf/playerbots.conf.dist | sort | uniq -d` → empty
- Every `#include` present on either side of a Group C/D conflict is present in the resolved file
- `GetInstanceStrategies()` contains all 5 added names

Build (user, then agent fixes):

- Full AzerothCore build with the module. **This is where #2576 bites**: our 141 new files were
  written against pre-#2576 headers and may fail on missing transitive includes. Expect a round or
  two of `error: ... has not been declared` / `incomplete type` and fix by adding the direct include
  to the file that needs it — do **not** re-add the include upstream removed.
- Paste compiler errors back; the agent fixes them in the working tree.

Database (user, before the runtime test):

- Incoming SQL must be applied to the world DB — `ai_playerbot_rpg_status_texts` (#2372), Korean
  `ai_playerbot_texts` (#2547), and the `id1` fix (#2586).

Runtime smoke test (user, after a green build):

- `.playerbots bot add <name>`, confirm bots spawn and no double-free on `.playerbots disable`
- Naxx: pull Noth (target switching across balcony phase), Maexxna (web wrap breaking), Faerlina
  (worshipper sacrifice) — these are the Group B files, most likely to have lost something
- Ulduar: any encounter, confirms the split-file layout survived the delete/modify resolution
- Loot: run a group kill with Group Loot and with Master Loot, confirm bots roll under the first and
  pass under the second
- `.playerbots autogear` and a levelup to confirm #2605/#2490 did not fight our gemming/tinker code
