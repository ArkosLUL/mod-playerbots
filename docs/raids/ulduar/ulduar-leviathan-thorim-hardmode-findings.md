# Ulduar Flame Leviathan Towers + Thorim Sif Hard-Mode — Server-Truth Findings

Phase 3 of [ulduar-hard-mode-plan.md](ulduar-hard-mode-plan.md) (step 3:
"Flame Leviathan towers + Thorim Sif"). Vezax (P1) and Iron Assembly (P2) done and
config-gated. Both bosses in this step are self-contained *added-danger* hard modes:
raid opts in (leaves towers up / reaches Thorim fast), server spawns extra ground
AoE, bots must dodge. Follower model — bots never trigger it.

Source: `src/server/scripts/Northrend/Ulduar/Ulduar/boss_flame_leviathan.cpp`,
`boss_thorim.cpp`.

---

## Flame Leviathan — active-tower mask

### Server truth (clean)

At pull, `ActivateTowers()` (`boss_flame_leviathan.cpp:482`) reads how many of the 4
towers the raid left standing and, per surviving tower, **adds an empower aura to the
boss (`me`)** plus schedules that tower's periodic ground event:

| Tower | Boss empower aura | Ground event NPC (target dummy) | Damage spell | Hazard shape |
|---|---|---|---|---|
| Storm | `SPELL_TOWER_OF_STORMS` 65076 | `NPC_THORIM_HAMMER_TARGET` 33364 (8 spawn) | `SPELL_THORIMS_HAMMER` 62911 | Static lightning strikes at 8 fixed marks, ~5s telegraph then beam |
| Flame | `SPELL_TOWER_OF_FLAMES` 65075 | `NPC_MIMIRONS_INFERNO_TARGET` 33369 | `SPELL_MIMIRONS_INFERNO` 62909 | Escort-path **moving** fire trail, drops fire every 2s |
| Frost | `SPELL_TOWER_OF_FROST` 65077 | `NPC_HODIRS_FURY_TARGET` 33108 (2 spawn) | `SPELL_HODIRS_FURY` 62533 | **Chases** a random player, stuns, drops frost AoE at catch point |
| Life | `SPELL_TOWER_OF_LIFE` 64482 | `NPC_FREYA_WARD` 33367 (adds) | — | Spawns attacking adds (kill, not dodge) |

Two independent detection signals, both usable:
- **Boss aura** — `boss->HasAura(SPELL_TOWER_OF_*)` tells exactly *which* towers are up
  (the mask). Boss entry 33113.
- **`GetData(DATA_GET_TOWER_COUNT=2)`** returns count only (not which). Aura check is
  strictly better for us, no cast needed.

Detector plan: `uint32 FlameLeviathanActiveTowerMask(botAI)` returns bitmask over the
4 auras (only when config on). Per-tower dodge triggers key off the matching
ground-effect NPC being near the bot's vehicle.

### Bot behaviour (vehicle-piloted dodge)

The whole fight is fought from vehicles (demolisher/siege/chopper). Existing
[UldActions_FlameLeviathan.cpp:88](../src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L88)
`MoveAvoidChasing()` already pilots the vehicle to arena `corners` to kite the boss —
same primitive works for dodging ground effects (`MoveTo`, `MovementPriority`).

- **Thorim's Hammer** (Storm): move vehicle >~10y from nearest `NPC_THORIM_HAMMER_TARGET`
  (33364) during its telegraph window.
- **Mimiron's Inferno** (Flame): keep vehicle away from `NPC_MIMIRONS_INFERNO_TARGET`
  (33369) — it's a moving trail, avoid its forward path.
- **Hodir's Fury** (Frost): when `NPC_HODIRS_FURY_TARGET` (33108) is chasing this bot,
  keep moving (it's a follow-and-drop; standing still = hit).
- **Freya's Ward** (Life): adds — attack/kill, no movement change. Likely lowest value
  since the vehicle DPS loop already targets nearest attacker.

---

## Thorim — Sif joins (arena hard mode)

### Server truth

Sif (`NPC_SIF` 33196) is summoned every pull (`boss_thorim.cpp:381`) and normally
channels a hologram, then despawns after the 150s dominion timer
(`EVENT_SIF_FINISH_DOMINION`, `boss_thorim.cpp:822`). If the raid clears the arena
gauntlet **fast enough**, `ACTION_SIF_JOIN_FIGHT` fires instead: Sif interrupts her
channel, joins, and (after a 9s taunt) starts casting in the final/arena phase
(`boss_thorim.cpp:829-852`):

- `SPELL_FROSTBOLT_VALLEY` — raid-wide frost volley (unavoidable, heal through)
- `SPELL_BLIZZARD` 62577 → summons `NPC_SIF_BLIZZARD` 32879, a **moving blizzard**
  ground AoE (30s), respawned every ~15s
- `SPELL_FROST_NOVA` 62605 — Sif teleports to a random spot then point-blank nova

### Detection candidates (no clean GetData)

Sif has no `GetData` hard-mode override. Two options:
1. **`NPC_SIF` (33196) alive AND not channeling hologram** (`SPELL_SIF_CHANNEL_HOLOGRAM`
   64324) — true once she joins. Requires reading her aura state.
2. **`NPC_SIF_BLIZZARD` (32879) alive** — only ever exists in hard mode. Simplest
   "dodge now" signal, but only present intermittently (every ~15s), so it can't gate
   the whole Sif-active state, only the blizzard-dodge trigger.

Proposed: detector `IsThorimHardModeActive(botAI)` = option 1 (Sif alive + joined);
blizzard-dodge trigger additionally keys on `NPC_SIF_BLIZZARD` presence.

### Bot behaviour

- **Sif's Blizzard**: move out of `NPC_SIF_BLIZZARD` (32879) radius (standard
  `MoveAwayFromCreature` / flee, on foot — Thorim arena is not vehicle).
- **Frost Nova**: ranged/healers stay >~12y from Sif so her teleport-nova misses.
- Frostbolt Volley: no movement (raid-wide), healers cope.

---

## Open questions (need answers before implementing)

**Scope / split**
1. This step bundles two bosses. Land them as **one session** (both files) or split
   into 3a (Leviathan) then 3b (Thorim)? Leviathan is the heavier lift (vehicle
   piloting), Thorim mirrors the Vezax/on-foot dodge pattern.

**Flame Leviathan**
2. **Which towers to handle?** All 4, or drop Life (Freya's Ward is adds, already
   covered by the vehicle's "kill nearest attacker" loop)? My read: implement Storm +
   Flame + Frost dodges; skip Life as no new behaviour.
3. **Config gating.** Same per-boss switch pattern → `AiPlayerbot.UlduarFlameLeviathanHardMode`
   (default 0), gate `FlameLeviathanActiveTowerMask` on it. Confirm.
4. **Dodge vs DPS tension.** The vehicle DPS/kite loop and a dodge action will compete
   for `MoveTo`. OK to give tower-dodge higher `MovementPriority` than the normal kite
   when a ground effect is within X yards, else defer to existing loop? (Recommended.)

**Thorim**
5. **Detector choice** — option 1 (Sif alive + not hologram-channeling) vs a simpler
   proxy. Confirm option 1.
6. **Config switch** `AiPlayerbot.UlduarThorimHardMode` (default 0). Confirm.
7. Handle **only Blizzard + Frost Nova** dodges (skip Frostbolt Volley as unavoidable)?
   My read: yes.

**Follower model**
8. Confirm no kill-order / focus enforcement for either boss (pure added-dodge only) —
   consistent with plan's follower model and the Vezax precedent.

## Decisions (locked with user)
1. **Split** — 3a Flame Leviathan first, 3b Thorim next session.
2. Skip Life tower (adds handled by vehicle DPS loop).
3. Per-boss config switches, default 0.
4. Tower-dodge outranks normal kite loop (`MOVEMENT_FORCED`).
5. Thorim detector = Sif alive + not hologram-channeling.
6. Thorim: dodge Blizzard + Frost Nova only.
7. **Enforce kill order** where a boss has one (overrides the earlier "pure dodge" read).
   N/A to Flame Leviathan (single boss). Revisit for Thorim 3b — arena gauntlet adds have
   an implicit priority; decide the concrete order there.

## 3a — Flame Leviathan: IMPLEMENTED
- Config `AiPlayerbot.UlduarFlameLeviathanHardMode` (default 0):
  [PlayerbotAIConfig.h](../src/PlayerbotAIConfig.h) / [.cpp](../src/PlayerbotAIConfig.cpp) /
  [playerbots.conf.dist](../conf/playerbots.conf.dist).
- Enums + tower bitmask + `ULDUAR_FL_TOWER_HAZARD_RADIUS` (18y) in
  [UldBossHelper.h](../src/Ai/Raid/Uld/Util/UldBossHelper.h).
- Detector `FlameLeviathanActiveTowerMask` (reads boss's `SPELL_FL_TOWER_OF_*` auras) +
  `GetFlameLeviathanNearestTowerHazard` (nearest Storm/Flame/Frost ground NPC, Life excluded)
  in [UldHardMode.cpp](../src/Ai/Raid/Uld/Util/UldHardMode.cpp).
- Trigger `flame leviathan tower hazard` + action (pilots vehicle out of nearest hazard,
  `MOVEMENT_FORCED`, `ACTION_RAID + 3`) — wired in `UldTriggerContext.h` / `UldActionContext.h`
  / [UldStrategy.cpp](../src/Ai/Raid/Uld/UldStrategy.cpp).
- codestyle-cpp.py: touched files clean (remaining failures are pre-existing core files).

## 3b — Thorim: IMPLEMENTED

**Server-truth correction (important):** the plan/findings proposed detecting Sif's
non-joined state via `SPELL_SIF_CHANNEL_HOLOGRAM` (64324). That spell is **defined but never
cast** in this core (`boss_thorim.cpp`) — the channel Sif actually casts is
`SPELL_TOUCH_OF_DOMINION` (62507) on Thorim, so that proxy is unusable. Real live signal: Sif
(`NPC_SIF` 33196) spawns at Thorim's throne (Z ~438) and only `NearTeleportTo`s onto the arena
floor once she interrupts her channel to join (`ACTION_SIF_JOIN_FIGHT`). Detector is therefore
**Sif alive AND `GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD` (429.6)** — reuses the
same floor threshold the existing Thorim strategy already uses to tell arena floor from throne.

- Config `AiPlayerbot.UlduarThorimHardMode` (default 0): `PlayerbotAIConfig.{h,cpp}`,
  `playerbots.conf.dist`.
- Enums `NPC_SIF` (33196), `NPC_SIF_BLIZZARD` (32879) + radii `ULDUAR_THORIM_SIF_BLIZZARD_RADIUS`
  / `ULDUAR_THORIM_SIF_FROST_NOVA_RADIUS` (both 12y) in `Util/UldBossHelper.h`.
- Detector `IsThorimHardModeActive` in `Util/UldHardMode.{h,cpp}`.
- Triggers `thorim sif blizzard trigger` (everyone, too close to `NPC_SIF_BLIZZARD`) and
  `thorim sif frost nova trigger` (ranged/healers only, too close to Sif), both gated on the
  detector; `MoveAwayFromCreatureAction` actions, wired in `UldTriggerContext.h` /
  `UldActionContext.h` / `UldStrategy.cpp` at `ACTION_RAID + 3`.
- Frostbolt Volley (62604, raid-wide) deliberately NOT handled — unavoidable, healed through.

## Verification (per plan)
- `python apps/codestyle/codestyle-cpp.py` (CI runs `-Werror`).
- In-game per boss: switch `0` → bots ignore towers/Sif (normal); `1` → bots dodge.
- Flame Leviathan: leave a tower up, enable switch, confirm bots steer the vehicle out of
  Thorim's Hammer / Inferno / Hodir's Fury; disable → bots eat them (normal mode).
