# Ulduar Hodir Hard-Mode — Findings & Implementation (DONE)

Part of the Ulduar hard-mode rollout (see `ulduar-hard-mode-plan.md`, phase 2). Implemented
2026-07-18. Extends the existing normal-mode Hodir strategy under `src/Ai/Raid/Uld/`.

## What Hodir's hard mode actually is

Unlike every other Ulduar hard mode, Hodir's is **not a new hazard or empowered ability**. It is the
**"Rare Cache of Winter" 3-minute timed kill**: `boss_hodir.cpp` schedules `EVENT_HARD_MODE_MISSED`
at 180 s; when it fires, Hodir shatters the hard-mode chest (`GO_HODIR_CHEST_HARD` 194200). Kill
threshold is HP < 150000 or a lethal blow. The fight is otherwise identical to normal mode — the raid
just needs **more DPS and fewer deaths** inside 180 s.

The way a raid beats the timer is by exploiting the **friendly helper NPCs** (Priest/Druid/Shaman/Mage,
faction-dependent). They spawn already flash-frozen and must be freed first, then grant:

- **Storm Cloud** (shaman, spell 65123, difficulty-mapped at runtime): lands on a random raid member
  every 30 s; while that carrier stands near allies it periodically triggers **Storm Power**
  (63711 / 65134 in 25m) — a crit-damage AoE around the carrier. The #1 DPS multiplier of the fight.
- **Starlight** (druid, area aura 62807): a ground haste zone. *(Not implemented — see Dropped.)*
- **Toasty Fire** (mage, `NPC_TOASTY_FIRE` 33342, aura 62821): standing in it prevents Biting Cold
  stacks and grants Flash-Freeze exemption (`SPELL_SAFE_AREA_TRIGGERED`).

## What normal-mode Hodir code already did (unchanged)

`Trigger/UldTriggers_Hodir.*` + `Action/UldActions_Hodir.*`, wired in `UldStrategy.cpp`:
- `hodir near snowpacked icicle` → `hodir move snowpacked icicle` (`ACTION_RAID+1`): during a Flash
  Freeze cast, stack behind a Snowpacked Icicle to block line-of-sight and avoid being frozen.
- `hodir biting cold` → `hodir biting cold jump` (`ACTION_RAID`): removes the Biting Cold player aura
  — **cheat-only** (`BotCheatMask::raid`); no legit handling before this work.
- `hodir frost resistance trigger/action` (generic `BossFrostResistance*`).

So survival vs Flash Freeze is partly covered; the DPS-race optimisation was entirely missing, and
Biting Cold had no non-cheat handling.

## Detection

`IsHodirHardModeActive(botAI)` in `Util/UldHardMode.{h,cpp}`, Freya combat-gate style:

```cpp
bool IsHodirHardModeActive(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.ulduarHodirHardMode)
        return false;
    Unit* hodir = GetFirstAliveUnitByEntry(botAI, NPC_HODIR);   // 32845, from core ulduar.h
    return hodir != nullptr && hodir->IsInCombat();
}
```

**`GetData(3)` deliberately NOT used.** It reports the 3-min timer (`bAchievCacheRare`), but the buff
optimisation is harmless even after the window is missed, and GetData reads are fragile (cf. the Vezax
`GetData(1)` gotcha in `ulduar-vezax-hardmode-config-plan.md`). Config + Hodir-in-combat is sufficient.

Config flag `ulduarHodirHardMode` (default off) added to `PlayerbotAIConfig.{h,cpp}` and
`conf/playerbots.conf.dist`, matching the other `Ulduar<Boss>HardMode` opt-ins.

## Bot behaviours added (3, all gated on `IsHodirHardModeActive`)

All three extend the existing `UldTriggers_Hodir.*` / `UldActions_Hodir.*` files (no new file pair),
mirroring how Freya's hard-mode classes were appended. Each action's `isUseful()` re-instantiates its
trigger and returns `IsActive()` (project convention).

| Trigger / Action | Priority | Behaviour |
|---|---|---|
| `hodir free frozen helper` | `ACTION_RAID` | A helper ice block (`NPC_HODIR_FLASH_FREEZE_BLOCK` 32938) is alive within 40 yd → `Attack()` it. Prerequisite: no freed helpers = no buffs. Mirrors `FreyaBreakIronRootsAction`. Wide search so it works wherever the raid stacked on engage. Sits below the snowpacked-icicle move (`ACTION_RAID+1`) so surviving a Flash Freeze always wins over running to a re-frozen helper. |
| `hodir spread storm cloud` | `ACTION_RAID` | Bot carries the Storm Cloud aura (`sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_CLOUD, bot)`) and has < 2 allies within `ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS` (10 yd) → move to the **nearest ally** (not the whole-raid centroid, which could be an empty midpoint between two groups) so Storm Power lands on a real cluster. |
| `hodir move to toasty fire` | `ACTION_RAID` | Bot has Biting Cold (`SPELL_BITING_COLD_PLAYER_AURA` 62039) and is not within `ULDUAR_HODIR_TOASTY_FIRE_RADIUS` (5 yd) of a `NPC_TOASTY_FIRE` (33342), and a fire exists within 60 yd → move to it. Legit no-cheat Biting-Cold mitigation; complements the cheat-only jump (on cheat servers the jump clears the aura first, so this stays idle). |

### Priority interaction
`hodir move snowpacked icicle` (`ACTION_RAID+1`, only during a Flash Freeze cast) owns
freeze-avoidance and strictly outranks helper-freeing (`ACTION_RAID`), so a bot never leaves
line-of-sight cover to chase a re-frozen helper mid-cast. Toasty Fire seeking only fires when Biting
Cold is actually ticking, so it does not thrash against normal positioning. Storm-Cloud spreading and
helper-freeing only fire under their own aura/target conditions.

## New ids / constants (`Util/UldBossHelper.h`)

- `NPC_HODIR_FLASH_FREEZE_BLOCK = 32938` — ice block encasing a frozen helper (boss-local in core,
  not in `ulduar.h`, so added to the `UlduarIDs` enum Hodir block).
- `SPELL_HODIR_STORM_CLOUD = 65123` — base id, difficulty-mapped at runtime.
- `ULDUAR_HODIR_TOASTY_FIRE_RADIUS = 5.0f`, `ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS = 10.0f`.
- `NPC_HODIR` (32845), `NPC_TOASTY_FIRE` (33342), `SPELL_BITING_COLD_PLAYER_AURA` (62039) already
  existed (former via core `ulduar.h`, latter two in the enum).

## Files touched

- `Util/UldHardMode.{h,cpp}` — `IsHodirHardModeActive`.
- `Util/UldBossHelper.h` — new ids + radius constants.
- `Trigger/UldTriggers_Hodir.{h,cpp}` — 3 trigger classes (+ `SpellMgr.h`, `UldHardMode.h` includes).
- `Action/UldActions_Hodir.{h,cpp}` — 3 action classes.
- `UldTriggerContext.h` / `UldActionContext.h` — creator + factory registrations.
- `UldStrategy.cpp` — 3 `TriggerNode`s in the Hodir block.
- `PlayerbotAIConfig.{h,cpp}`, `conf/playerbots.conf.dist` — `ulduarHodirHardMode` flag.

Codestyle clean (`apps/codestyle/codestyle-cpp.py`; pre-existing Aq40/Naxx failures untouched). Not
built (slow C++ build; build only on request).

## Dropped / tunable

- **Starlight standing** dropped from scope (user decision): area-aura detection is fiddlier and the
  haste gain is lower-impact than the three shipped behaviours.
- **Radii are conservative guesses** — Storm Power and Toasty Fire radii are DBC, not in the server
  script. `ULDUAR_HODIR_TOASTY_FIRE_RADIUS` and `ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS` are tunables
  to confirm in-game.

## Verification (manual, user-driven)

Enable `AiPlayerbot.UlduarHodirHardMode = 1`, pull Hodir with a bot raid, confirm:
- bots free the flash-frozen helpers at the pull (attack the ice blocks, then return to Hodir);
- a Storm-Cloud carrier moves into the raid stack;
- bots step into a Toasty Fire once Biting Cold ticks on them;
- with the flag off, normal mode is unchanged (no regressions).
