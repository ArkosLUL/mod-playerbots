# Ulduar Hard-Mode Support — Implementation Plan

## Context

Playerbot Ulduar raid AI lives in `src/Ai/Raid/Uld/`. After the recent refactor
(commit `e93c2965`) the strategy is split per boss:
`Action/UldActions_<Boss>.{h,cpp}`, `Trigger/UldTriggers_<Boss>.{h,cpp}`,
registered in `UldActionContext.h` / `UldTriggerContext.h`, wired in
`UldStrategy.cpp`, with shared enums/positions/helpers in `Util/UldBossHelper.h`.

The current strategies handle only the **normal (easy) version** of each encounter.
Ulduar hard modes are *optional harder versions* of a fight — activated by a raid
choice (leave towers up, keep Saronite Vapors alive, hit Mimiron's red button,
leave Elders alive, reach Thorim fast, reduce Yogg Keepers). They are **not** the
10/25 heroic difficulty flag, so the "heroic comes free via spell-id predicate"
trick from other raids does not apply here.

The gap was already catalogued in `docs/ulduar-boss-strategy-gap-analysis.md`
(Severity-3 table). That doc's Sev-1 normal-mode fixes are the phase currently in
progress (the modified Algalon/Hodir/Razorscale/Auriaya files in git status); this
plan is the **Sev-3 hard-mode phase**. Today there is **zero** hard-mode code except
one 10/25 passenger-count check at `UldActions_FlameLeviathan.cpp:398`.

**Confirmed design decisions (from the user):**
- **Follower model / auto-detect only.** Bots never *trigger* a hard mode themselves.
  They react to hard mode once the server-side encounter marks it active (extra adds
  spawn, empowering auras present, boss `GetData` flag set). A player/raid opts in;
  bots handle the added mechanics. (In an all-bot raid nobody triggers hard mode — acceptable.)
- **Detection source: server boss AI `GetData`, in-process,** plus hard-mode-only
  NPC/GO/aura presence. Bots share the worldserver process, so
  `creature->AI()->GetData(id)` is valid and authoritative.
- **Scope: shared detection infra first, then all 8 Sev-3 bosses, phased.**

Intended outcome: an Ulduar bot raid that, when a human triggers any hard mode,
correctly recognises it and executes the added mechanics instead of playing the
encounter as if it were normal mode.

---

## Approach

### Step 0 — Shared hard-mode detection layer (build first)

Add a small detection module — `Util/UldHardMode.{h,cpp}` (or extend
`UldBossHelper`) — with one detector per encounter. Each returns the *current
server truth*, so triggers stay thin. Reuse `GetFirstAliveUnitByEntry`
(`RaidBossHelpers.h:21`) to find the boss/add, then
`->ToCreature()->AI()->GetData(id)` where the server exposes a flag:

- `bool IsHodirHardModeActive(botAI)` — Hodir `GetData` hard-mode flag + berserk/3-min
  timer awareness (server: `EVENT_HARD_MODE_MISSED` 3min, `EVENT_BERSERK` 8min in
  `boss_hodir.cpp`).
- `bool IsVezaxHardModeActive(botAI)` — Vezax `GetData(1)` (`hardmodeAvailable`) **and**
  Saronite Animus (NPC 33524) alive (`boss_general_vezax.cpp`).
- `uint32 FreyaActiveElderMask(botAI)` — which of Brightleaf/Ironbranch/Stonebark are
  still alive during the Freya fight.
- `bool IsThorimHardModeActive(botAI)` — Sif present / boss hard-mode `GetData` +
  arena kill-timer.
- `bool IsMimironFirefighterActive(botAI)` — Firefighter aura/`GetData` (self-destruct
  button pressed).
- `uint32 FlameLeviathanActiveTowerMask(botAI)` — which of Storm/Flame/Frost/Life
  towers are still up.
- `bool IsAssemblySteelbreakerEmpowered(botAI)` — Steelbreaker alive with the other two
  dead / Supercharge stacks (empowered ability set).
- `uint32 YoggActiveKeeperMask(botAI)` — active Keeper bitmask (server instance
  `PERSISTENT_DATA_WATCHERS_MASK`, exposed via instance `GetData`).

These detectors are the only new coupling to server internals; every per-boss trigger
calls them rather than re-deriving state.

### Step 1..8 — Per-boss hard-mode mechanics

Each new behaviour is the **same 5 edit sites**, now in the per-boss files:
1. `Trigger/UldTriggers_<Boss>.{h,cpp}` — new `Trigger` subclass; `IsActive()` calls
   the Step-0 detector then checks the specific mechanic.
2. `Action/UldActions_<Boss>.{h,cpp}` — new `Action` subclass; `Execute()` behaviour.
3. `UldTriggerContext.h` + `UldActionContext.h` — register creator strings.
4. `UldStrategy.cpp` `InitTriggers` — `TriggerNode` + `ACTION_*` priority
   (avoidance = `ACTION_EMERGENCY`; focus/positioning = `ACTION_RAID`).
5. `Util/UldBossHelper.h` — add the missing NPC/GO/spell enums + any positions.

**Reuse (do NOT rebuild):**
- Cone/facing dodge: `IsBotInFrontalCone` (`RaidBossHelpers.h:24`).
- Ground/puddle avoidance: existing `MoveAwayFromCreature` / `FleePosition` patterns
  (already used by Vezax shadow-crash & Mimiron rocket-strike actions).
- Add lookup: `GetFirstAliveUnitByEntry` (`RaidBossHelpers.h:21`).
- Focus fire / CC: `SetRtiTarget(botAI,"skull",t)` / `SetRtiCcTarget` — bots kick/interrupt
  via class rotations; the raid layer only needs to mark priority targets.
- Real tank swap: `RazorscaleBossHelper::AssignRolesBasedOnHealth` role-swap machinery
  (`UldBossHelper.cpp:198`) + `GetGroupAssistTank`.

#### Per-boss hard-mode deltas (what the bot must add)

| Boss | Detected state | Added bot behaviour |
|---|---|---|
| **Vezax** | Saronite Animus (33524) alive | Skull-mark + focus the Animus; dodge its Profound Darkness AoE. Simplest hard mode — pure "new add appears, kill it". Add `NPC_SARONITE_ANIMUS` enum. |
| **Hodir** | Hard-mode flag, 3-min cache timer not yet missed | Maximise DPS/heal uptime: stand in Toasty Fire (33342), stack captured-NPC buffs, minimise Flash-Freeze/Biting-Cold downtime. No new boss ability — a race. Depends on the Sev-1 Flash-Freeze/Biting-Cold fixes landing first. |
| **Assembly of Iron** | Steelbreaker empowered (last alive) | Handle Steelbreaker's empowered kit: tank-swap on Fusion Punch, dodge Meltdown/Electrical Charge at low HP, kill order via marked focus. |
| **Flame Leviathan** | Active tower mask | Per active tower, dodge its zone AoE from the vehicle: Storm→Thorim's Hammer strikes, Flame→Mimiron's Inferno chase-fire, Frost→Hodir's Fury, Life→Freya's adds. Add tower NPC/GO enums. |
| **Thorim** | Sif present / arena hard mode | Dodge Sif's Blizzard/Frost Nova in the arena phase; keep kill-speed (follow marked focus). |
| **Mimiron** | Firefighter active | All-phase persistent ground-fire cell avoidance; add + avoid Frost Bomb (currently not enum'd); kill Emergency Bots. Builds on the Sev-1 fire-avoidance work. |
| **Freya** | Elder mask (1–3 alive) | Per living Elder, handle its empower: Brightleaf→dodge Unstable Sun Beam/Solar Flare, Ironbranch→break Iron Roots, Stonebark→dodge Ground Tremor knockback. **Add the 3 Elder NPC enums** (missing today — structural blocker). |
| **Yogg-Saron** | Active Keeper mask (fewer = harder) | With fewer Keeper support buffs, mandatory sanity discipline: tighter Sanity Well usage, Lunatic Gaze look-away, Malady dispel focus, brain-phase execution — no leaning on Keeper crutches. Most subtle; largest test surface. |

### Phased implementation order (ascending complexity / risk)

1. **Infra (Step 0)** + **Vezax Animus** (reference boss — smallest, self-contained delta). **DONE**
2. **Hodir** race + **Assembly** Steelbreaker kit. Assembly **DONE**; Hodir pending.
3. **Flame Leviathan** towers + **Thorim** Sif. **DONE**
4. **Mimiron** Firefighter + **Freya** Elders. Freya **DONE** (mechanics-only: break Iron Roots,
   dodge Unstable Sun Beam — see `ulduar-freya-hardmode-findings.md`); Mimiron pending.
5. **Yogg-Saron** reduced-Keeper discipline (largest, most test-heavy — last).

Each numbered step is a discrete session: research the exact server `GetData` id
meanings and spell ids in `src/server/scripts/Northrend/Ulduar/Ulduar/boss_*.cpp`,
extend this doc / a per-boss findings note, confirm open questions, then implement.

---

## Phase 1 — DONE: detection infra (Step 0) + Vezax

**Detection module:** `Util/UldHardMode.{h,cpp}` created with the first detector
`IsVezaxHardModeActive(botAI)`. Future bosses add their detector here.

**Server-truth correction (important):** the plan's proposed detector
`GetData(1) && Animus alive` is wrong. Vezax `GetData(1)` returns `lootMode == 3`,
which the boss only sets in `DoAction(2)` — i.e. *after* the Saronite Animus **dies**
(hard mode completed). It is not a live "active" signal. The real live truth is simply
**Saronite Animus (NPC 33524) alive**: it spawns once six Saronite Vapors are reached
uncleared, and Vezax gains the invulnerable Saronite Barrier until it dies. Detector
uses `GetFirstAliveUnitByEntry(botAI, NPC_VEZAX_SARONITE_ANIMUS)`.

**Bot behaviour added:**
- `vezax saronite animus trigger` → `... action` (`ACTION_RAID + 1`): everyone switches
  target to and kills the Animus (no RTI mark — each bot `Attack()`s it directly).
- `vezax profound darkness trigger` → `... action` (`ACTION_RAID + 2`): ranged/healers
  (`PlayerbotAI::IsRanged`) move out of the Animus' Profound Darkness (63420); melee
  keep tanking it. Avoid radius is a tunable constant
  `ULDUAR_VEZAX_PROFOUND_DARKNESS_RADIUS = 15.0f` in `UldBossHelper.h` — exact spell
  radius not in the server script (DBC), so this is a conservative default to confirm
  in-game.
- New enum `NPC_VEZAX_SARONITE_ANIMUS = 33524` in `UldBossHelper.h`.

Files touched: `Util/UldHardMode.{h,cpp}` (new), `Util/UldBossHelper.h`,
`Trigger/UldTriggers_Vezax.{h,cpp}`, `Action/UldActions_Vezax.{h,cpp}`,
`UldTriggerContext.h`, `UldActionContext.h`, `UldStrategy.cpp`. Codestyle clean.

## Verification

Builds are slow — do **not** build unless asked. To validate end-to-end per boss:
- Enable `RaidUlduarStrategy`, spawn a bot raid on a local worldserver, and trigger
  each hard mode **with a human** (leave towers up, keep vapors, press Mimiron's
  button, leave Elders, free fewer Keepers), then confirm bots switch to the
  hard-mode behaviour (kill the Animus, dodge tower/Sif/Firefighter AoE, break Iron
  Roots, manage sanity) rather than playing it as normal mode.
- Confirm the follower model: in an all-bot raid with no trigger, bots run the normal
  version unchanged (no regressions on already-handled mechanics).
- Verify detectors read live server truth (kill/spawn the flagged add mid-fight and
  confirm the trigger flips).
- Run `python apps/codestyle/codestyle-cpp.py` before claiming done.
