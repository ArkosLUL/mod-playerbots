# Ulduar Hard Mode — Config-Gated Activation

## Context

Ulduar hard-mode bot behaviour previously activated **automatically** whenever the
server marked the encounter's hard mode live (for Vezax: the Saronite Animus NPC 33524
is alive). The detector `IsVezaxHardModeActive()` in
[UldHardMode.cpp](../src/Ai/Raid/Uld/Util/UldHardMode.cpp) returned pure server truth with
no opt-in.

The user wanted hard-mode strategy to kick in **only when a config switch is on**, not
purely from auto-detection. Decision: **per-boss switches** (Vezax now, future bosses add
their own). The switch is opt-in (default off), so an all-bot raid that stumbles into hard
mode does not silently change behaviour unless the operator enabled it.

## Approach

Add one per-boss bool config option and gate the boss's hard-mode detector on it. The
detector stays the single choke point, so every hard-mode trigger/action that already
routes through it is gated for free.

### 1. Config option `AiPlayerbot.UlduarVezaxHardMode`

Follows the existing bool-option pattern (e.g. `battleRezBossOnly`).

- **[PlayerbotAIConfig.h](../src/PlayerbotAIConfig.h)** — `bool ulduarVezaxHardMode;`
  next to `battleRezBossOnly`.
- **[PlayerbotAIConfig.cpp](../src/PlayerbotAIConfig.cpp)** —
  `ulduarVezaxHardMode = sConfigMgr->GetOption<bool>("AiPlayerbot.UlduarVezaxHardMode", false);`
- **[conf/playerbots.conf.dist](../conf/playerbots.conf.dist)** — documented, default `0`.

### 2. Gate the detector

In [UldHardMode.cpp](../src/Ai/Raid/Uld/Util/UldHardMode.cpp), early-return when the switch
is off:
```cpp
bool IsVezaxHardModeActive(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.ulduarVezaxHardMode)
        return false;

    return GetFirstAliveUnitByEntry(botAI, NPC_VEZAX_SARONITE_ANIMUS) != nullptr;
}
```
Added the `PlayerbotAIConfig.h` include so `sPlayerbotAIConfig` resolves. Header comment in
[UldHardMode.h](../src/Ai/Raid/Uld/Util/UldHardMode.h) updated: detectors now return
"config-enabled **and** server truth".

### 3. Route the second Vezax trigger through the detector

[VezaxSaroniteAnimusTrigger::IsActive()](../src/Ai/Raid/Uld/Trigger/UldTriggers_Vezax.cpp)
checked `GetFirstAliveUnitByEntry(NPC_VEZAX_SARONITE_ANIMUS)` **directly**, bypassing the
config gate. Now gates on `IsVezaxHardModeActive(botAI)` first, keeping the raw lookup only
for the "current target != animus" comparison.
`VezaxProfoundDarknessTrigger` already called the detector — no change.

## Why this shape

Config check lives in the detector, not scattered across triggers — matches the
"detectors are the only new coupling to server internals; triggers stay thin" principle in
[ulduar-hard-mode-plan.md](ulduar-hard-mode-plan.md). Future bosses add
`bool ulduar<Boss>HardMode;` + a gate line at the top of their own `Is<Boss>HardModeActive()`.

## Verification

- `python apps/codestyle/codestyle-cpp.py` (must pass; CI runs `-Werror`).
- Build worldserver (only if asked — builds are slow).
- In-game: `AiPlayerbot.UlduarVezaxHardMode = 0`, pull Vezax, let Animus spawn — bots
  ignore it (normal mode). Set `= 1`, repeat — bots switch target to the Animus and
  ranged/healers dodge Profound Darkness. Toggling mid-fight flips behaviour on the next
  trigger tick.
