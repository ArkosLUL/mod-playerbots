# Merge `brighton-chi/mod-playerbots@sunwell-strategies` into `Custom`

## Context

The Sunwell Plateau (SWP) bot strategies live on a third-party fork branch,
`https://github.com/brighton-chi/mod-playerbots` @ `sunwell-strategies` (`ccc2e8e6`). It is not
configured as a remote in this clone, which is why the branch looked inaccessible — the repo is
public and `git ls-remote` against it succeeds, and `gh` is authenticated as ArkosLUL.

Goal: land those strategies on `Custom` with conflicts resolved, without disturbing Custom until the
result has been reviewed.

### Divergence (measured via the GitHub compare API, no fetch performed)

- Merge base: `5640dc38` — already an ancestor of `Custom`.
- `sunwell-strategies`: 15 commits, 36 files changed vs. that base.
- 31 files are **new** (`src/Ai/Raid/SWP/**`, ~11k lines, plus one SQL update) — no conflict possible.
- 5 files are **modified**; `src/Script/Playerbots.cpp` is untouched on the Custom side, so it merges
  clean. The remaining **4 are the conflicts**.

Every conflict is the same shape: Custom added AQ40/ToC registrations to a list, sunwell adds SWP
registrations to the same list, in adjacent or identical hunks. Nothing is semantically opposed.

Compile risk is low: Custom's post-base edits to the shared headers SWP depends on
(`AttackAction.h`, `MovementActions.h`, `RaidBossHelpers.*`, `PlayerbotAI.h`) are additive only, and
the `HasTargetExclusions()` / `AppendTargetExclusions(GuidSet&, TargetValueExclusionType)` API that
`RaidSunwellStrategy` overrides is present in Custom (`src/Bot/Engine/Strategy/Strategy.h:76-78`).
The module has no `CMakeLists.txt` — AzerothCore collects sources by glob, so `src/Ai/Raid/SWP/` is
picked up with no build-file edit.

## Steps

### 1. Set up the remote and branch

```bash
git remote add brighton https://github.com/brighton-chi/mod-playerbots.git
git fetch brighton sunwell-strategies
git switch -c merge/sunwell-strategies Custom
git merge brighton/sunwell-strategies
```

Working tree is currently clean on `Custom` (tracking `origin/Custom`). The merge will stop with
conflicts in the 4 files below.

### 2. Resolve the conflicts

Rule for all four: **union — keep both sides' entries.** Sunwell's entries go where its diff placed
them (SWP sits after `zulaman` in the TBC block); Custom's AQ40/ToC entries stay put.

**`src/Ai/Raid/RaidStrategyContext.h`** — three additions:
- `#include "SWPStrategy.h"` after `#include "ZAStrategy.h"`
- `creators["sunwell"] = &RaidStrategyContext::sunwell;` after the `zulaman` creator
- `static Strategy* sunwell(PlayerbotAI* botAI) { return new RaidSunwellStrategy(botAI); }` in the
  private block, before `rs`

**`src/Bot/Engine/BuildSharedActionContexts.cpp`**:
- `#include "SWPActionContext.h"` after `ZAActionContext.h`
- `actionContexts.Add(new RaidSunwellActionContext());` after the `RaidZulAmanActionContext` line

**`src/Bot/Engine/BuildSharedTriggerContexts.cpp`**:
- `#include "SWPTriggerContext.h"` after `ZATriggerContext.h`
- `triggerContexts.Add(new RaidSunwellTriggerContext());` after the `RaidZulAmanTriggerContext` line

**`src/Bot/PlayerbotAI.cpp`** — the only non-mechanical one. Custom refactored
`ApplyInstanceStrategies` into `GetInstanceStrategies()` + `IsInstanceStrategy()`
([PlayerbotAI.cpp:1621-1640](modules/mod-playerbots/src/Bot/PlayerbotAI.cpp#L1621-L1640)); sunwell
only reflowed the old inline literal and added two things. **Keep Custom's refactor entirely** and
graft sunwell's additions onto it:

- Add `"sunwell"` to the list in `GetInstanceStrategies()` (alphabetical, between `"ssc"` and
  `"tbc-ac"`). Also add back `"rs"` in the same edit — see step 3.
- Add the map case in `ApplyInstanceStrategies`, between `case 578` (Oculus) and `case 595`
  (Culling of Stratholme):

```cpp
        case 580:
            strategyName = "sunwell";  // Sunwell Plateau
            break;
```

Discard sunwell's reflow of the literal — it was formatting against the pre-refactor version.

### 3. Fold in the `rs` fix

Custom's `GetInstanceStrategies()` list is missing `"rs"` even though
[PlayerbotAI.cpp:1764](modules/mod-playerbots/src/Bot/PlayerbotAI.cpp#L1764) still maps map 724 to
it, so the Ruby Sanctum strategy is applied but never removed on zone change. Sunwell's side of the
conflict still carries `"rs"`. Restore it in the same list edit as `"sunwell"`.

Resulting list block:

```cpp
        "aq20", "aq40", "blacktemple", "bwl", "gruulslair", "hyjal", "icc", "karazhan",
        "magtheridon", "moltencore", "naxx", "onyxia", "rs", "ssc", "sunwell", "tbc-ac",
        "tempestkeep", "trialofthecrusader", "ulduar", "voa", "wotlk-an", "wotlk-cos", "wotlk-dtk",
        "wotlk-eoe", "wotlk-fos", "wotlk-gd", "wotlk-hol", "wotlk-hor", "wotlk-hos", "wotlk-nex",
        "wotlk-occ", "wotlk-ok", "wotlk-os", "wotlk-pos", "wotlk-toc", "wotlk-uk", "wotlk-up",
        "wotlk-vh", "zulaman"
```

### 4. Take the clean side of everything else

`src/Script/Playerbots.cpp` (forward decl + call of `AddSC_SunwellPlateauBotScripts()`), the 31 new
files, and `data/sql/playerbots/updates/2026_07_04_00_ai_playerbot_sunwell_texts.sql` merge without
intervention. Confirmed no filename collision in the SQL updates directory (latest existing there is
`2026_06_19_00_...`).

## Verification

The module cannot be compiled headless from this clone, so verification is static plus a hand-off
build. In order:

1. `git diff --check` and `grep -rn '<<<<<<<\|>>>>>>>\|=======' src/` — no leftover markers.
2. `git diff Custom...HEAD --stat` — expect exactly the 36 files from the compare, nothing else.
3. Symbol wiring check — each name registered in the four resolved files resolves to a real
   definition on the merged tree:
   - `RaidSunwellStrategy` → `src/Ai/Raid/SWP/SWPStrategy.h`
   - `RaidSunwellActionContext` → `src/Ai/Raid/SWP/SWPActionContext.h`
   - `RaidSunwellTriggerContext` → `src/Ai/Raid/SWP/SWPTriggerContext.h`
   - `AddSC_SunwellPlateauBotScripts` → defined in `src/Ai/Raid/SWP/Util/SWPScripts.cpp`
4. Check every `#include` in `src/Ai/Raid/SWP/**` resolves against the merged tree — this is the main
   place a 106-commit divergence could bite.
5. Grep the SWP sources for calls into `RaidBossHelpers` / `PlayerbotAI` / `MovementActions` /
   `AttackAction` and confirm each signature still matches on Custom.
6. Hand off for a real build (`./acore.sh compiler build` or the usual local cmake flow). Only after
   that passes: merge `merge/sunwell-strategies` into `Custom`.

## Notes

- No commit, push, or merge into `Custom` happens without you asking for it — step 6's final merge is
  a separate go-ahead.
- In-game smoke test once built: bots in Sunwell Plateau (map 580) should report the `sunwell`
  strategy active, and it should clear on leaving the instance.
