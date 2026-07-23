# Restrict Bot Battle Resurrection to Boss Fights

## Context

Bots currently cast **Rebirth** (Druid combat/battle resurrection) whenever they are in
combat and any party member is dead — including on trash packs. Battle rez is a scarce,
high-value cooldown that should be conserved for boss encounters. This change gates the
in-combat rez trigger so bots only battle-rez during a boss fight, and adds a config toggle
so servers can restore the old always-on behavior.

**Scope confirmation:** Two bot-controlled battle-rez paths are gated:
1. Druid **Rebirth** — the `combat party member dead` trigger (`CombatPartyMemberDeadTrigger`),
   registered only in the Druid combat strategy.
2. Shaman **Reincarnation** (self-res) — the `can self resurrect` trigger
   (`SelfResurrectTrigger`) → `SelfResurrectAction`.

Warlock Soulstone is a pre-applied buff that auto-fires on death (left as-is). The self-res
trigger is *shared* by Reincarnation and Soulstone-recipient self-res, so we gate **only**
when the self-res spell is Reincarnation. Confirmed in core: `Player.cpp:12783-12784` —
`HasSpell(20608)` sets `PLAYER_SELF_RES_SPELL = 21169`; Soulstone stores a different spell id.
Out-of-combat rezzes (Revive/Redemption/Resurrection/Ancestral Spirit) are unaffected.

## Decisions (confirmed with user)

- **Detection = union:** treat it as a boss fight if EITHER an instance encounter is
  `IN_PROGRESS` OR any current group attacker is `IsDungeonBoss() || isWorldBoss()`. Covers
  scripted dungeon/raid encounters plus world bosses that lack an instance script.
- **Config toggle:** new `AiPlayerbot.BattleRezBossOnly` (default `1` / true). When false,
  restores old behavior (battle rez any time a party member dies in combat).

## Changes

### 1. Add config option
- `src/PlayerbotAIConfig.h` — declare `bool battleRezBossOnly;` (near the other combat toggles).
- `src/PlayerbotAIConfig.cpp` — load it:
  `battleRezBossOnly = sConfigMgr->GetOption<bool>("AiPlayerbot.BattleRezBossOnly", true);`
- `conf/playerbots.conf.dist` (module config template) — add a documented entry defaulting to 1.

### 2. Add a reusable boss-fight helper
Add a small static helper so the logic is testable/reusable rather than inlined in the trigger.
Put it next to the existing attacker logic:
- `src/Ai/Base/Value/AttackersValue.h` / `.cpp` — add
  `static bool IsInBossFight(PlayerbotAI* botAI);`

Implementation combines the two existing patterns already in the codebase:
- Instance path — mirror `ShamanTriggers.cpp:191-202`:
  if `bot->GetMap()->IsDungeon()`, get `InstanceScript`, loop
  `GetEncounterCount()` and return true on any `GetBossState(i) == IN_PROGRESS`.
- Attacker path — mirror `GenericTriggers.cpp:294`:
  iterate the `attackers` guid list (`AI_VALUE(GuidVector, "attackers")`), resolve each unit,
  return true if any `creature->IsDungeonBoss() || creature->isWorldBoss()`.
- Return false otherwise.

### 3. Gate the combat rez trigger
- `src/Ai/Base/Trigger/HealthTriggers.cpp` — update `CombatPartyMemberDeadTrigger::IsActive()`:
  ```cpp
  bool CombatPartyMemberDeadTrigger::IsActive()
  {
      if (!GetTarget())
          return false;
      if (sPlayerbotAIConfig.battleRezBossOnly && !AttackersValue::IsInBossFight(botAI))
          return false;
      return true;
  }
  ```
  Add the `AttackersValue.h` include if not already present.

`PartyMemberDeadTrigger` (out-of-combat `resurrect`/Revive) is intentionally **left untouched**.

### 4. Gate Shaman Reincarnation (self-res)
`SelfResurrectTrigger::IsActive()` is currently an inline body in `GenericTriggers.h:954-960`.
Move the body to `GenericTriggers.cpp` (keep includes out of the header) and gate the
Reincarnation case only:
```cpp
bool SelfResurrectTrigger::IsActive()
{
    if (bot->IsAlive())
        return false;
    uint32 const resSpell = bot->GetUInt32Value(PLAYER_SELF_RES_SPELL);
    if (!resSpell)
        return false;
    // Reincarnation (21169): conserve for boss fights; Soulstone self-res unaffected.
    constexpr uint32 SPELL_REINCARNATION_SELF_RES = 21169;
    if (resSpell == SPELL_REINCARNATION_SELF_RES &&
        sPlayerbotAIConfig.battleRezBossOnly && !AttackersValue::IsInBossFight(botAI))
        return false;
    return true;
}
```
Notes:
- While the bot is a corpse, `IsInBossFight` still works: the `attackers` value aggregates the
  *group's* attackers (so the boss is seen while groupmates fight), and the instance-encounter
  `IN_PROGRESS` path is independent of the bot being alive. If the whole group has wiped and the
  encounter reset, Reincarnation correctly does **not** fire (nothing to res into).

## Files touched
- `src/PlayerbotAIConfig.h`, `src/PlayerbotAIConfig.cpp`
- `conf/playerbots.conf.dist`
- `src/Ai/Base/Value/AttackersValue.h`, `src/Ai/Base/Value/AttackersValue.cpp`
- `src/Ai/Base/Trigger/HealthTriggers.cpp` (and include `AttackersValue.h` where needed)
- `src/Ai/Base/Trigger/GenericTriggers.h` (declare `IsActive()`), `GenericTriggers.cpp` (define it)

## Verification
- Build worldserver with the module (only if explicitly asked — builds are slow).
- Run `python apps/codestyle/codestyle-cpp.py` for style.
- In-game / behavioral check:
  - Pull a **trash pack** in a dungeon, let a bot die → Druid bot should **not** cast Rebirth.
  - Start a **boss** encounter, let a bot die → Druid bot **should** cast Rebirth.
  - Aggro a **world boss** (no instance script), kill a bot → Rebirth should still fire (attacker path).
  - **Shaman Reincarnation:** kill a Shaman bot (Reincarnation off cd) on **trash** → should NOT
    self-res; kill it during a **boss** fight → should Reincarnate.
  - Confirm a Shaman bot holding a **Warlock Soulstone** still self-reses on trash (not gated).
  - Set `AiPlayerbot.BattleRezBossOnly = 0`, repeat trash tests → Rebirth + Reincarnation fire on
    trash (old behavior).
