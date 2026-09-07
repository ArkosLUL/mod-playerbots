# Merge upstream `test-staging` into `Custom`

## Context

`origin/test-staging` (== `upstream/test-staging`, tip `075242fd1`) has 23 new commits since the
last integration. `Custom` has 297 local commits on top of the same merge base (`a1252c6d3`),
almost all of them raid-boss AI under `src/Ai/Raid/`. The goal is to land the 23 upstream commits
on `Custom` without losing any local raid work and without leaving the tree in a state that fails
to compile.

Note: the local `test-staging` branch is stale — it still points at the old merge base `a1252c6d3`.
The merge source is `origin/test-staging`.

## What's coming in

89 files, +5134/-606. Highlights that matter to `Custom`:

| Commit | Why it matters here |
|---|---|
| `8c000dfca` feat(mgt) | New Magisters' Terrace dungeon strategy (~3400 lines, all new files) |
| `dd369c427` rogue | New `GenericRogueStrategy` class; restructures Dps/Assassination rogue |
| `b99419cc2` | Renames `WarrirorAoeStrategy` |
| `1e064b459` priest | Fear Ward on MT + new trigger registrations in `GenericPriestStrategy` |
| `5b71503aa` | `AttackersValue::IsPossibleTarget()` changes — affects raid targeting |
| `9f13694c9` | `MoveFarTo` guarded against ground moves mid-flight |
| `02207b556` / `164335fe9` | Large `PlayerbotFactory` rewrite (professions) + zone concentration |
| `075242fd1` | CI promotes warnings to errors |

`Seth/SethData.h` is renamed to `SethShared.h`.

Sources are picked up by AzerothCore's module glob — there is no `CMakeLists.txt` in this module,
so the new MgT files need no build wiring.

## Conflicts

`git merge-tree` (read-only dry run) reports **9 conflicted files**, all small:

1. `src/Ai/Base/Actions/WipeAction.cpp`
2. `src/Ai/Class/Priest/PriestActions.h`
3. `src/Ai/Class/Priest/PriestTriggers.h`
4. `src/Ai/Class/Priest/Strategy/GenericPriestStrategy.cpp`
5. `src/Ai/Class/Rogue/RogueAiObjectContext.cpp`
6. `src/Bot/Factory/PlayerbotFactory.cpp`
7. `src/Bot/PlayerbotAI.cpp`
8. `src/Util/EncounterHelpers.cpp`
9. `src/Util/EncounterHelpers.h`

19 further files touched by both sides auto-merge, including `conf/playerbots.conf.dist`
(purely additive upstream) and `PlayerbotFactory.h`.

### Resolutions

The governing rule for this merge: **upstream's 23 commits contain almost nothing that competes with
Custom's raid work.** Most conflicts are "upstream reformatted / deleted a blank line next to a line
Custom rewrote". Where a hunk looks symmetric, it usually isn't — check the merge base before
assuming both sides added something.

#### 1. `WipeAction.cpp` — combine, upstream's guard first

Upstream adds an `IsAlive()` guard (#2742, "wipe shouldn't try to kill ghosts"). Custom adds a
`RaidObs::NoteScriptedWipe(bot)` call that must precede `Kill`. Order matters: the guard goes first,
so an already-dead bot doesn't get filed as a scripted wipe.

```cpp
    if (!bot->IsAlive())
        return false;

    // Before the kill: Unit::Kill runs the death hook inline, so the flag has to be set by the time
    // it does or the record is already written.
    RaidObs::NoteScriptedWipe(bot);

    bot->Kill(bot, bot);

    return true;
```

#### 2. `PriestActions.h` — keep Custom's `15.0f`, adopt upstream's wrapping

The third arg is `estAmount` (estimated heal as % of target HP), not a range. `ConserveManaStrategy`
vetoes heals where `lossAmount < estAmount`; `25.0f` suppressed Penance above 75% target HP. Custom
changed it to `15.0f` in `2ba4b0dba`, documented as finding D7 in `docs/classes/priest.md:84-85` and
listed at line 104 under "do not re-investigate". Upstream's `25.0f` is untouched base — it only
re-wrapped the line.

#### 3. `PriestTriggers.h` — take Custom wholesale

Base already had `BOOST_TRIGGER_A(ShadowfiendTrigger, ...)` and `BUFF_TRIGGER(InnerFocusTrigger, ...)`.
Upstream changed neither. Custom replaced both with `SpellNoCooldownTrigger` subclasses. Taking
upstream loses `InnerFocusTrigger`'s declaration (still constructed at `PriestAiObjectContext.cpp:182`)
and leaves `ShadowfiendTrigger::IsActive()` undefined — Custom deleted that definition from
`PriestTriggers.cpp`, which auto-merges cleanly.

Upstream's deletion of `FearWardTrigger` / addition of `FearWardOnMainTankTrigger` lands outside the
conflict and is safe: Custom's raid anti-fear (`RaidAntiFear.cpp`, `BWL*`, `TK*`) casts `"fear ward"`
by spell name, never through those context entries.

#### 4. `GenericPriestStrategy.cpp` — take Custom, then add **only** the fear-ward node

Everything else on upstream's side is unchanged base context that Custom deliberately moved into the
spec ladders (`HealPriestStrategy`, `HolyPriestStrategy`, `ShadowPriestStrategy`) — a derived
`InitTriggers` cannot drop a base node, so per-spec pricing has to start out of the generic strategy.
Taking upstream's side double-registers shadowfiend, inner focus, hymn of hope and the PW:S bands.

```cpp
    triggers.push_back(new TriggerNode("fear ward on main tank",
                                       { NextAction("fear ward on main tank", ACTION_HIGH + 3) }));
```

The supporting `FearWardOnMainTankTrigger` / `CastFearWardOnMainTankAction` / context creators
auto-merge in regardless; omitting this node would leave them dead and drop fear ward outside raid
scripts. **Open question below on the priority value.**

#### 5. `RogueAiObjectContext.cpp` — three hunks, the third is a trap

Hunks 1 and 2 are pure formatting-vs-addition: upstream renamed `ai` → `botAI` and wrapped at 100
columns; Custom added `"tricks of the trade"` and `"tricks of the trade on main tank and light aoe"`.
Upstream deleted nothing — it never had those entries. Keep both sides.

Hunk 3 is asymmetric. Git already auto-merged upstream's reformatted `use_instant_poison`,
`use_deadly_poison` and `use_instant_poison_off_hand` into the lines *immediately above* the conflict,
and Custom's side re-lists them. Taking Custom → three duplicate definitions. Taking upstream → loses
`tricks_of_the_trade`, which hunk 2's creator references. Keep only `tricks_of_the_trade` from Custom,
then upstream's three tail lines:

```cpp
    static Action* tricks_of_the_trade(PlayerbotAI* botAI)
    {
        return new CastTricksOfTheTradeAction(botAI);
    }
    static Action* fan_of_knives(PlayerbotAI* botAI) { return new FanOfKnivesAction(botAI); }
    static Action* killing_spree(PlayerbotAI* botAI) { return new CastKillingSpreeAction(botAI); }
    static Action* cold_blood(PlayerbotAI* botAI) { return new CastColdBloodAction(botAI); }
```

Post-resolution check: `use_instant_poison`, `use_deadly_poison` and `use_instant_poison_off_hand`
must each appear exactly twice in the file (one creator, one definition).

Upstream's new `GenericRogueStrategy` does **not** move Custom's tricks registrations — it registers
only the two poison nodes. Leave `DpsRogueStrategy.cpp:130-132` and
`AssassinationRogueStrategy.cpp:126-128` alone; both files auto-merge cleanly.

#### 6. `PlayerbotFactory.cpp` — keep both, twice

Includes: keep `<limits>` (Custom) and `<unordered_set>` (upstream).

Anonymous namespace: both sides appended functions and share the trailing `}` `}`. Concatenate —
Custom's `GetEnchantIdOfSpell` + `GetRuneforgeEnchantId` (closing the latter with an explicit `}`),
then upstream's `HasCreatureSpawnRow`, whose close is the pre-existing brace.

#### 7. `PlayerbotAI.cpp` — union, both hunks

Strategy list: insert `"tbc-mgt"` into Custom's alphabetical list between `"tbc-mech"` and
`"tbc-seth"`. Custom-only entries `aq40`, `sunwell`, `trialofthecrusader` stay.

Map switch: keep both cases with their own `break;` — the conflict swallowed the shared one.

```cpp
        case 580:
            strategyName = "sunwell";  // Sunwell Plateau
            break;
        case 585:
            strategyName = "tbc-mgt";  // Magisters' Terrace
            break;
```

#### 8. `EncounterHelpers.h` — keep both

Upstream only appends `// DO NOT USE, WILL BE REMOVED` to the `GetFirstAliveUnitByEntry` declaration;
Custom inserts `IsDownOrFeigning` right after it. Adjacent, not competing.

```cpp
Unit* GetFirstAliveUnitByEntry(PlayerbotAI* botAI, uint32 entry);  // DO NOT USE, WILL BE REMOVED
// Feign death (Stalagg/Feugen) keeps the creature alive at 1 HP but unselectable and lying down,
// so IsAlive() on its own no longer means "still up".
bool IsDownOrFeigning(Unit const* unit);
```

#### 9. `EncounterHelpers.cpp` — keep both

Custom added `IsBotInFrontalCone`; upstream added a full stop to the comment below it. Keep Custom's
function, take upstream's punctuation.

Upstream changed **no** function signature or body in this file — `GetFirstAliveUnitByEntry`'s
implementation is byte-identical. Custom's `IsBotInFrontalCone` (4 callers) and `IsDownOrFeigning`
(14 callers) are untouched by upstream, and none of upstream's new symbols
(`BOSS_ENGAGED_HEALTH_PCT`, `BOSS_BURN_HEALTH_PCT`, `GetStepToPosition`) collide with anything in the
99 files doing `using namespace EncounterHelpers`.

### Formatting note

`.clang-format` is byte-identical across all three sides at `ColumnLimit: 120`. Upstream wraps at 100,
violating the repo's own config. Adopt upstream's wrapping **inside the conflicted hunks only**, to
reduce churn on the next merge. Do not reformat anything else.

## Post-merge fixes — required, in the same commit

These are breaks the merge introduces that git cannot see. They compile cleanly.

### F1. Rogues lose `reach melee` (severe, every raid)

Upstream re-parents `DpsRogueStrategy` and `AssassinationRogueStrategy` from `MeleeCombatStrategy`
to the new `GenericRogueStrategy`, which derives from `CombatStrategy`. `MeleeCombatStrategy.cpp:14-16`
is the only thing that gives rogues `"enemy out of melee"` → `reach melee`; `CombatStrategy` has no
such node, and `AiFactory.cpp:377-381` never adds the `"close"` strategy to rogues. Both headers are
Custom-untouched, so upstream's base-class swap applies unconditionally.

Custom's own comment at `DpsRogueStrategy.cpp:209` and `AssassinationRogueStrategy.cpp:189` spells
out the dependency: *"Has to outrank the inherited 'reach melee' on the same trigger."* After the
merge that inherited node is gone and rogues stand still whenever Sprint is on cooldown.

Fix — re-parent the new upstream class (two lines):

```cpp
// GenericRogueStrategy.h
#include "MeleeCombatStrategy.h"                              // was CombatStrategy.h
class GenericRogueStrategy : public MeleeCombatStrategy       // was CombatStrategy

// GenericRogueStrategy.cpp
    MeleeCombatStrategy::InitTriggers(triggers);              // was CombatStrategy::InitTriggers
```

`GetType()` needs no change — it calls `CombatStrategy::GetType()`, still a base, and already ORs in
`STRATEGY_TYPE_MELEE`. The constructor's `CombatStrategy(botAI)` delegation does need to become
`MeleeCombatStrategy(botAI)`.

This looks like a genuine upstream bug — worth reporting to mod-playerbots separately.

### F2. Priority collision on 26.0

Upstream's `GenericRogueStrategy` adds `use instant poison on main hand` at **26.0** and
`use deadly poison on off hand` at **25.5**. Custom's `tricks of the trade` is `ACTION_HIGH + 6` =
**26.0** in both rogue specs — an exact tie, so ordering is unspecified.

Raise tricks to `ACTION_HIGH + 6.5f` (26.5) at `DpsRogueStrategy.cpp:132` and
`AssassinationRogueStrategy.cpp:128`. Resulting ladder: tricks 26.5, MH poison 26.0, OH poison 25.5,
slice and dice 25.0, rupture 24.0.

### F3. Fear ward priority

Register the node at `ACTION_HIGH + 0.7f` (20.7), not upstream's `ACTION_HIGH + 3` (23.0), so it sits
below the whole Disc ladder (penance/shadowfiend 22, PW:S/inner focus 21) and never preempts them.

### F4. Stale comment

`ArmsWarriorStrategy.cpp:193` references `WarrirorAoeStrategy`, which upstream renames to
`WarriorAoeStrategy`. Comment only — the rename itself is clean (every other occurrence is inside the
three files upstream rewrites).

## Behaviour changes to be aware of (no action)

- **`AttackersValue::IsPossibleTarget`** (#2708) does **not** add feign-death, unattackable or
  non-selectable filtering — those checks live above the changed hunk at `AttackersValue.cpp:151-160`
  and are untouched. Stalagg/Feugen, Thaddius, Razorscale and Freya's elders behave as today. The one
  widening is a new `IsInCombatWith(bot)` gate, which makes `GetFirstAliveUnitByEntry` (159 Custom
  call sites) resolve mobs slightly earlier. Worth eyeballing the densest phase gates in play —
  `UldEncounter_Mimiron.cpp:413-449`, `UldTriggers_Mimiron.cpp`, `ToCTriggers.cpp` — where a `nullptr`
  return was used as an implicit "not engaged yet".
- **`MoveFarTo` flight guard** (#2710) touches only the RPG subsystem. Zero callers under
  `src/Ai/Raid/`, and `MovementActions.{cpp,h}` is unchanged, so Custom's 547 `MoveTo` sites are safe.
- **`GetFirstAliveUnitByEntry` is now marked for removal upstream.** Not this merge's problem, but 159
  call sites will break when upstream follows through. Consider a Custom-owned copy under a different
  name so the eventual deletion is a no-op.

## Out of scope — follow-ups, not this merge

- **60 warnings in Custom-only code.** Upstream's `075242fd1` promotes warnings to errors via
  `CXXFLAGS` in three CI workflow YAMLs only — no CMake changes anywhere in the merge range, so the
  local build is unaffected and the merge itself costs nothing. But five CI jobs go red on the first
  upstream PR: 45 `-Wunused-parameter` (mostly `Execute(Event event)` overrides that ignore `event`,
  and free helpers taking an unused `PlayerbotAI* botAI` in `Uld/Util/UldEncounter_*.cpp`), 6
  `-Wunused-variable`, plus a handful of others. That count is clang-only and a floor: gcc adds
  `-Wmaybe-uninitialized`/`-Warray-bounds`, and MSVC's `-WX` will error on all 45 as C4100 since the
  `-wd` list doesn't cover it. The fix is mechanical (delete parameter names) but it is a separate,
  large, mergeable-anytime change.
- **Real pre-existing bug at `src/Ai/Raid/SWP/Action/SWPActions.cpp:99`:**
  `if (PlayerbotAI::IsTank && ...)` is missing its call parentheses — `IsTank` is
  `static bool IsTank(Player*, bool)`, so this takes the function's address and is always true. The
  Sunwell Eredar Twins tank gate has never worked. Unrelated to the merge; worth its own fix.
- **`"tricks of the trade on main tank and light aoe"`** is registered but no `TriggerNode` consumes
  it, unlike the Hunter's equivalent at `GenericHunterStrategy.cpp:73`. Possibly an unfinished
  feature. Keep it — dropping it is out of scope here.

## Execution

Copy this plan to `docs/plans/merge-test-staging-20260907/merge-test-staging-20260907.PLAN.md` first.

```bash
git switch -c merge/test-staging-20260907 Custom
git merge origin/test-staging          # expect 9 conflicted files
```

**`rerere` is enabled with 123 cached resolutions**, so some hunks will arrive pre-resolved from
earlier test-staging merges. Do not trust them: run `git rerere status` and `git rerere diff`, and
diff every auto-staged hunk against the resolutions above before accepting.

Resolve the 9 files, apply F1–F4, then verify, then commit with `git commit -F <file>` (never inline
a multi-line message). No push.

## Verification

1. **Conflict markers gone:** `git diff --check` and
   `grep -rn '^<<<<<<<\|^>>>>>>>' src/` returns nothing.
2. **Nothing dropped:** in `RogueAiObjectContext.cpp`, each of `use_instant_poison`,
   `use_deadly_poison`, `use_instant_poison_off_hand` appears exactly twice (one creator, one
   definition); `RogueValueContextInternal` and its `valueContexts.Add(...)` registration survive;
   `"tricks of the trade"`, `"tricks of the trade on main tank and light aoe"`, `"assassination
   rupture"`, `"killing spree"` and `"envenom"` creators are all still present.
3. **Per-TU compile check.** No full build is feasible here, but the prebuilt
   `acore/ac-wotlk-build:master` image carries `/azerothcore/build/compile_commands.json` from a real
   Ninja configure, and `apps/docker/Dockerfile:53` sets `CWITH_WARNINGS=ON`, so its baked flags are
   already the exact CI warning set. All 761 playerbot entries share one flag set, so an entry can be
   synthesized for the 7 files new to this merge. Drive it with `-fsyntax-only` over the merge-touched
   `.cpp` files:
   ```bash
   git diff --name-only a1252c6d3 075242fd1 | grep '\.cpp$' | tr -d '\r' > "$SP/files.txt"
   MSYS_NO_PATHCONV=1 docker run --rm \
     -v "G:/DevStuff/GitHub/azerothcore-wotlk-pb/modules/mod-playerbots/src:/azerothcore/modules/mod-playerbots/src:ro" \
     -v "$SP:/work:ro" --entrypoint python3 acore/ac-wotlk-build:master /work/wscan.py
   ```
   ~48 TUs, about 2 minutes at 12-way parallelism. Caveat: no link step, so ODR clashes and missing
   symbols slip through — which matters here, because the `PriestTriggers.h` resolution is exactly a
   missing-symbol risk. Grep `PriestTriggers.cpp` directly to confirm `InnerFocusTrigger::IsActive`
   is defined and no `ShadowfiendTrigger::IsActive` reference survives.
4. **Codestyle gates** (cheap, local, and part of CI):
   `python apps/codestyle/codestyle-cpp.py` from the module root, plus
   `cppcheck --force --inline-suppr --suppressions-list=./.suppress.cppcheck src/`.
5. **In play**, once built: a rogue chasing a target with Sprint on cooldown must still close to melee
   (F1), and a priest's Disc ladder must still open with penance/shadowfiend rather than fear ward (F3).
