# Obsidian Sanctum (Sartharion) — 3-Drake Strategy Hardening + Difficulty Config

## Context

The playerbots Obsidian Sanctum strategy (`src/Ai/Raid/OS/`, strategy id `"wotlk-os"`,
auto-assigned by map 615 at `PlayerbotAI.cpp:1734`) only reliably does **Sarth+0**: it
tunnels all three drakes down first (`SartharionAttackPriorityAction`, hardcoded
vesperon→tenebron→shadron→sartharion) then kills Sartharion. There is **no config** to
choose a harder mode, and several drake mechanics are unhandled, so leaving any drake alive
(the whole point of Obsidian Sanctum — Sarth+1/+2/+3, the "Twilight Zone" tier) wipes the raid.

Goal: (1) a config letting the user pick how many drakes stay alive (0/1/2/3), and (2) a
strategy solid enough to survive **Sarth+3** — tank all drakes, burn Sartharion, and handle
every add/portal/ground mechanic that currently kills bots.

**Encounter model used throughout:** "leave N drakes alive" = bots *tank* those N drakes but
never kill them, and burn Sartharion. Killing Sartharion ends the encounter and despawns the
drakes, so surviving drakes cost nothing but must be held and their mechanics serviced.

**Locked decisions (user):**
- Keep-order (which drakes stay up) = **Tenebron → Shadron → Vesperon**. So kill-order =
  Vesperon → Shadron → Tenebron. Leave1 keeps Tenebron; Leave2 keeps Tenebron+Shadron.
- Scope = **full overhaul** (all gaps below).

## Gaps found in current code (each a wipe vector at +1/+2/+3)

| # | Gap | Location |
|---|-----|----------|
| A | No difficulty config; kill-all-drakes is hardcoded | `OSActions.cpp:155-180` |
| B | DPS attack-priority kills the drakes we want kept | `OSActions.cpp:165-174` |
| C | Portal entry hard-gated off "drakes still alive" → acolytes never cleared when drakes kept; only Shadron's acolyte handled, not Vesperon's; exit checks only `acolyte of shadron` | `OSTriggers.cpp:99-105,122-125`, `OSActions.cpp:161,185,201-216` |
| D | No Twilight Egg / Whelp handling (Tenebron) — enum IDs unused | `OSTriggers.h:52-53` |
| E | No Lava Blaze handling (Sartharion Lava Strike adds) — enum ID unused | `OSTriggers.h:60` |
| F | Melee rear-flank only fires when **no** drake alive → melee eat Flame Breath/Cleave/Tail Lash in every leave-alive run | `OSTriggers.cpp:65-77` |
| G | One off-tank spot, no facing control; MT TankFace multiplier is a commented no-op → Tail Lash/Cleave/drake breath point into raid | `OSActions.cpp:50-80`, `OSMultipliers.cpp:25-28` |
| H | Ranged/healer stack point (`SARTHARION_RANGED_POSITION`) defined but never used | `OSActions.h:14` |
| I | Twilight Revenge risk: killing a to-kill drake while its acolyte/portal is still open buffs Sartharion massively | not handled anywhere |

## Design

### 1. Config (mirror the Ulduar hard-mode pattern exactly)
Single int, range 0–3, default 0.

- **`src/PlayerbotAIConfig.h`** (~line 268, next to `ulduarVezaxHardMode`): `int32 sartharionDrakesAlive;`
- **`src/PlayerbotAIConfig.cpp`** (~line 690, next to the Ulduar `GetOption` block):
  `sartharionDrakesAlive = std::clamp(sConfigMgr->GetOption<int32>("AiPlayerbot.SartharionDrakesAlive", 0), 0, 3);`
- **`conf/playerbots.conf.dist`** (~line 390, after the Ulduar block): doc comment (explain
  0=kill all, 1/2/3=leave that many up, keep-order Tenebron→Shadron→Vesperon) + `AiPlayerbot.SartharionDrakesAlive = 0`.

Read anywhere via `sPlayerbotAIConfig.sartharionDrakesAlive`.

### 2. New helper header `src/Ai/Raid/OS/OSShared.h` (inline, RS-style — no new .cpp/context wiring)
`namespace ObsidianSanctumHelpers`:
- `keepOrder[] = {TENEBRON, SHADRON, VESPERON}` and derivation from `sartharionDrakesAlive`:
  `IsDrakeKept(entry)` (first N of keepOrder), `IsDrakeToKill(entry)` (the rest).
- `FindDrakeToKill(botAI)` → highest kill-priority (Vesperon>Shadron>Tenebron) living drake
  in the kill-set, scanning `possible targets no los` (reuse `GetFirstAliveUnitByEntry` /
  the `RsFindTarget`-style scan; check `RaidBossHelpers` first).
- Add predicates: `AnyTwilightAddAlive` (egg 30882 / whelp 30890), `AnyLavaBlazeAlive` (30643),
  `AcolyteAliveFor(drakeEntry)` (acolyte of shadron 31218 / vesperon 31219).
- `DrakeAcolyteClear(drakeEntry)` — true if that drake has no live acolyte (gates Gap I).
- Reuse existing generic helpers rather than re-rolling: `RaidBossHelpers.h` —
  `IsBotInFrontalCone`/`GetPositionOutsideFrontalCone` (:26-27), `SetRtiTarget`/`MarkTargetWithSkull` (:8-19).

### 3. Attack priority — difficulty + realm + revenge aware (`SartharionAttackPriorityAction`, rewrite)
Target selection, first match wins:
1. **In twilight realm** (`bot->HasAura(SPELL_TWILIGHT_SHIFT)`): acolyte of shadron, else
   acolyte of vesperon. (Currently only shadron — Gap C.)
2. **Twilight whelps/eggs** alive and near raid → kill (eggs before they hatch). (Gap D.)
3. **Lava Blaze** alive near raid → kill. (Gap E.)
4. **To-kill drake** alive AND its acolyte cleared (`DrakeAcolyteClear`) → kill by kill-order.
   The acolyte-clear gate prevents Twilight Revenge (Gap I); Tenebron has no acolyte so it's
   always clear.
5. Else **Sartharion**.
Kept drakes are never selected. Same action serves realm and ground because it's driven by
`find target` name lookups, which resolve to whatever the bot can currently see.

### 4. Portal / acolyte rework (Gap C) — repeatable, both drakes
- `TwilightPortalEnterTrigger`: **remove** the "don't enter until drakes dead" aura gate
  (`OSTriggers.cpp:99-105`). Enter when: a `GO_TWILIGHT_PORTAL` is open nearby **and** an
  acolyte needs killing (boss has `SPELL_GIFT_OF_TWILIGHT_FIRE` [Shadron] or the Vesperon
  realm is open / `SPELL_TWILIGHT_TORMENT_VESPERON` present) **and** bot is a designated
  portal-runner (keep the existing "not MT / not assist-heal index 0" exclusion; optionally
  send 1 DPS too). Because Shadron/Vesperon stay alive, acolytes respawn and this re-fires
  each cycle — logic stays stateless so repeat entry just works.
- `EnterTwilightPortalAction`: enter on Gift-of-Twilight-**Fire** *or* the Vesperon realm
  marker, not Fire-only (`OSActions.cpp:185`).
- `TwilightPortalExitTrigger`: exit when **both** acolytes are gone (add `acolyte of vesperon`
  to the check at `OSTriggers.cpp:124`).

### 5. Off-tank multi-drake holding + facing (Gap G)
- `SartharionTankPositionAction`: off-tank/assist-tank picks up **every** landed drake (kept
  and to-kill) via the existing pre-aggro `possible targets no los` scan, force-threats them
  (`RsForceThreat` pattern: `AddThreat` + `FixateTarget`), and parks at `SARTHARION_OFFTANK_POSITION`
  — far from the raid stack. Hold all 3 at one spot is acceptable since raid/AoE is at the boss.
- Enable facing: MT faces Sartharion **away** from raid (Tail Lash/Cleave), OT faces drakes
  away from raid (Shadow Breath). Un-comment/implement the MT `TankFaceAction` branch in
  `OSMultipliers.cpp:25-28` and add an OT face; or drive facing in the tank-position action.

### 6. Melee & ranged positioning (Gaps F, H)
- `SartharionMeleePositioningTrigger`: drop the `!(drakes alive)` condition
  (`OSTriggers.cpp:76`). Fire whenever a melee DPS is attacking Sartharion → rear-flank the
  boss (avoid frontal Flame Breath + Cleave). When attacking a to-kill drake, rear-flank that
  drake (its Shadow Breath is frontal too).
- New `SartharionRangedPositioningTrigger` → move ranged/healers to the unused
  `SARTHARION_RANGED_POSITION` stack, giving tsunami-dodge and drake-breath a stable baseline.

### 7. New adds triggers/actions (Gaps D, E)
- Trigger `twilight adds` (whelp/egg present) and `lava blaze` (blaze present) → route through
  the reworked attack-priority (steps 2–3), or dedicated small kill actions. Whelps/eggs and
  Lava Blaze must be *killed*, not avoided.

### 8. Strategy + context wiring (`OSStrategy.cpp`, `OSTriggerContext.h`, `OSActionContext.h`)
Add `TriggerNode`s for the new triggers (twilight adds, lava blaze, ranged positioning) and
register every new trigger/action name in the two Context headers (name→factory maps). No
changes needed at the 3 outside registration sites (`RaidStrategyContext.h`,
`BuildSharedTriggerContexts.cpp`, `BuildSharedActionContexts.cpp`) — the strategy name is
unchanged. Priorities: avoid mechanics `ACTION_RAID+n`, positioning `ACTION_MOVE+n`, add/boss
targeting `ACTION_RAID`, portal `ACTION_RAID+1` (as today).

## Files touched
- **New:** `src/Ai/Raid/OS/OSShared.h`
- **Edit (behavior):** `OSTriggers.{h,cpp}`, `OSActions.{h,cpp}`, `OSMultipliers.cpp`,
  `OSStrategy.cpp`, `OSTriggerContext.h`, `OSActionContext.h`
- **Edit (config):** `src/PlayerbotAIConfig.h`, `src/PlayerbotAIConfig.cpp`,
  `conf/playerbots.conf.dist`

## Known caveats to encode
- Kept drakes are parked away from the boss stack so boss-centered AoE won't accidentally kill
  them (would forfeit the achievement, not wipe). Note in code comment.
- Aura→effect exact mapping (Gift of Twilight Fire vs Twilight Torment / Gift of Twilight
  Shadow 57835 vs Twilight Torment 57935) should be confirmed against DBC/spell data during
  implementation; enum IDs at `OSTriggers.h:16-50` are the starting point, several currently unused.

## Verification
No headless compile is possible in this environment — deliver a static pass + hand-off:
1. `grep`-confirm the config triple (`.h` member, `.cpp` `GetOption`, `.conf.dist` key) and that
   every new trigger/action name is registered in its Context header.
2. Confirm no new strategy name → the 3 outside wiring sites stay untouched.
3. In-game QA matrix (user's server), for `SartharionDrakesAlive` = 0/1/2/3:
   - Correct drakes killed vs kept (Tenebron→Shadron→Vesperon keep-order).
   - OT holds all kept drakes off the raid; MT/OT facing away.
   - Acolytes of both Shadron and Vesperon get cleared in the portal on every respawn; Gift of
     Twilight Fire / Twilight Torment drop off the raid.
   - Whelps/eggs and Lava Blaze die; melee never stand in front of boss/drakes.
   - Sarth+3: full clear without wipe.
