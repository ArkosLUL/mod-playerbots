# Ulduar hard modes: config-driven, no runtime detection

## Context

Today every Ulduar hard-mode detector in `src/Ai/Raid/Uld/Util/UldHardMode.cpp` returns
**config-enabled AND a live server signal** (extra add alive, empower aura present, Sif's Z
position, instance persistent data). That second half is fragile: it depends on this core's
scripting details (the Vezax `GetData(1)` and Sif-channel cases are already documented dead ends),
it silently disables the whole boss' hard-mode handling when a signal is missing, and it is
invisible to the server owner who explicitly turned the option on.

The eight `AiPlayerbot.Ulduar*HardMode` options are already opt-in and default off. They should be
the single source of truth: when the option is on, that boss' hard-mode handling is armed for the
whole encounter. The per-boss triggers keep their own mechanic checks (hazard nearby, debuff on the
bot, add alive), so nothing fires when there is nothing to react to — the mechanic checks are the
real gate, the detectors were a redundant second one.

Not in scope: the strategy wiring (`UldStrategy.cpp` pushes all hard-mode trigger nodes
unconditionally already) and encounter phase detection, which stays dynamic.

## Changes

### 1. `src/Ai/Raid/Uld/Util/UldHardMode.cpp` — detectors become config reads

| Function | New body |
|---|---|
| `IsVezaxHardModeActive` | `return sPlayerbotAIConfig.ulduarVezaxHardMode;` |
| `IsIronAssemblyHardModeActive` | `return sPlayerbotAIConfig.ulduarIronAssemblyHardMode;` |
| `IsThorimHardModeActive` | `return sPlayerbotAIConfig.ulduarThorimHardMode;` |
| `IsFreyaHardModeActive` | `return sPlayerbotAIConfig.ulduarFreyaHardMode;` |
| `IsHodirHardModeActive` | `return sPlayerbotAIConfig.ulduarHodirHardMode;` |
| `IsMimironHardModeActive` | `return sPlayerbotAIConfig.ulduarMimironHardMode;` |
| `IsYoggSaronHardModeActive` | `return sPlayerbotAIConfig.ulduarYoggSaronHardMode;` |
| `FlameLeviathanActiveTowerMask` | `return sPlayerbotAIConfig.ulduarFlameLeviathanHardMode ? FL_TOWER_ALL : 0;` |

Deleted outright (no longer needed, no other callers):
- `YoggActiveKeeperMask` and the file-static `GetBotInstanceScript` — the only `InstanceScript`
  dependency in the raid goes away.
- `YoggThorimKeeperActive` — with the Thorim-keeper case assumed whenever Yogg hard mode is on, it
  would just duplicate `IsYoggSaronHardModeActive`.

Kept dynamic (these are *phase*/target selection, not hard-mode detection):
- `IsSteelbreakerEmpowered` — still requires Steelbreaker alive with Molgeim and Brundir dead; the
  tank swap must not arm before phase 3. Its `IsIronAssemblyHardModeActive` guard now just reads the
  config.
- `GetIronAssemblyNextKillTarget`, `GetFlameLeviathanNearestTowerHazard` — unchanged.

Drop the now-unused `InstanceScript.h` and `Map.h` includes. `UldScripts.h` stays (still supplies
`NPC_STEELBREAKER`).

### 2. `src/Ai/Raid/Uld/Util/UldBossHelper.h`

- Add `FL_TOWER_ALL = 0xF` to `FlameLeviathanTowerFlags` (~[UldBossHelper.h:219-225](src/Ai/Raid/Uld/Util/UldBossHelper.h#L219-L225)).
- Remove the ids that become unreferenced: `SPELL_FL_TOWER_OF_STORMS/FLAMES/FROST/LIFE`,
  `NPC_FLAME_LEVIATHAN`, `SPELL_EMERGENCY_MODE`. Verify with grep before deleting each; the
  `NPC_FL_*_TARGET` hazard ids and `ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD` stay (still used by the
  hazard search and by Thorim's non-hard-mode code).

### 3. Yogg-Saron call sites — drop the keeper branch

- [UldTriggers_YoggSaron.cpp:649](src/Ai/Raid/Uld/Trigger/UldTriggers_YoggSaron.cpp#L649) →
  `if (!IsYoggSaronHardModeActive(botAI) || !IsPhase3())`
- [UldActions_YoggSaron.cpp:157-158](src/Ai/Raid/Uld/Action/UldActions_YoggSaron.cpp#L157-L158) →
  `if (botAI->HasCheat(BotCheatMask::raid) && !IsYoggSaronHardModeActive(botAI))`

Update the two comments there so they say the config drives it, not the keeper mask.

### 4. Behaviour notes to keep in mind (no code change needed)

Every other call site already re-checks its own mechanic, so removing the detector half is
behaviour-preserving whenever the raid really is running the hard mode:

- Vezax triggers re-check the Animus is alive ([UldTriggers_Vezax.cpp:87](src/Ai/Raid/Uld/Trigger/UldTriggers_Vezax.cpp#L87), and the Profound Darkness one is a proximity check on the Animus).
- Freya / Mimiron triggers key off objects that only exist in their hard mode (Iron Roots aura, Unstable Sun Beam, Flames nodes, Frost Bomb).
- Hodir triggers key off Flash Freeze blocks / Storm Cloud / Biting Cold, which also exist in normal mode — with the option on, bots run the DPS-race behaviour on every Hodir kill. That is the intended semantics.
- Flame Leviathan hazards are found by NPC entry, so towers the raid actually destroyed contribute nothing even though the mask claims all four.
- Thorim's ranged-avoid-Sif trigger now also applies while Sif stands at the throne. She is above the arena floor and the radius is 12 yd, so bots in the arena are not affected; if it turns out to matter in play, re-add a plain "Sif is below `ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD`" check inside `ThorimSifFrostNovaTrigger` (a positional relevance check, not hard-mode detection).

### 5. Docs

- Rewrite the header block in [UldHardMode.h:9-19](src/Ai/Raid/Uld/Util/UldHardMode.h#L9-L19): hard
  modes are declared by config, not detected; with the option on the handling is armed for the whole
  encounter and each trigger's own mechanic check decides when it fires. Trim the per-function
  comments that describe the removed live signals (Vezax `GetData` note, Sif Z note, Emergency Mode
  aura, keeper bitmask), keeping the part that explains what the hard mode *is*.
- [conf/playerbots.conf.dist:353-418](conf/playerbots.conf.dist#L353-L418): keep one block per boss,
  but fix wording that implies detection. Each block should state that the option must match what
  the raid actually does, since bots no longer detect it — e.g. the Yogg block's "With it off (or
  with all 4 Keepers) the fight plays as normal mode" must lose the keeper half, and add that the
  Thorim-Keeper handling is assumed. Defaults stay `0`.

## Verification

No unit-test harness for the module and it cannot be compiled headless here, so:

1. `git grep -n "YoggThorimKeeperActive\|YoggActiveKeeperMask\|GetBotInstanceScript\|SPELL_FL_TOWER\|SPELL_EMERGENCY_MODE\|NPC_FLAME_LEVIATHAN"` — expect zero hits outside the conf/doc prose.
2. `git grep -n "InstanceScript" src/Ai/Raid/Uld` — expect zero hits.
3. Confirm each remaining `Is*HardModeActive` body is a single config read, and that
   `IsSteelbreakerEmpowered` still has its alive checks.
4. Build the core with the module (`mod-playerbots` compiled into the AzerothCore build) — hand off
   if no local toolchain.
5. In-game, per boss with the matching option set to 1: pull the boss in **normal** mode and confirm
   bots run the hard-mode behaviour anyway (that is the point of the change) — Vezax bots switch to
   an Animus the moment one exists, Iron Assembly gets the skull kill order from the pull, Mimiron/
   Freya bots dodge the hazards as soon as any spawn, Hodir bots free helpers and sit in Toasty
   Fires, Yogg bots stop cheat-killing Crusher Tentacles and Immortal Guardians.
6. With every option back at 0, confirm the fights play exactly as before the change (no hard-mode
   trigger fires).
