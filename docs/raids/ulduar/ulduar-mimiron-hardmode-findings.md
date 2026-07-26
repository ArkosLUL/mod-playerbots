# Ulduar Mimiron "Firefighter" Hard-Mode — Server-Truth Findings

Phase 4 of [ulduar-hard-mode-plan.md](ulduar-hard-mode-plan.md) ("Mimiron Firefighter +
Freya Elders"); this doc covers **Mimiron**. Freya is done; Yogg-Saron is the last
remaining boss after this. Follower model — bots never press the button.

Source: `src/server/scripts/Northrend/Ulduar/Ulduar/boss_mimiron.cpp`.

Config: `AiPlayerbot.UlduarMimironHardMode` (default 0).

---

## Server truth

Mimiron hard mode = "Firefighter". A player presses the **Big Red Button**
(`GO_BUTTON 194739`) before the pull; its gossip handler runs only while the encounter is
`NOT_STARTED`, calls `SetData(_, 7)` on Mimiron (which sets `_hardmode = true`) and pulls
the boss. `_hardmode` is set at activation, never cleared for the rest of the fight, and
`GetData(1)` returns it (the Firefighter achievement is gated on exactly this).

**Detection choice:** `GetData(1)` is authoritative but lives on Mimiron himself
(`NPC_MIMIRON 33350`), who sits in his pod and never becomes a bot attack target, so he
never enters the `"possible targets"` lists the detectors scan. The clean **live** signal
is instead the empower aura firefighter puts on the currently-active mech:

| Firefighter adds | id | Bot-relevant hazard |
|---|---|---|
| `SPELL_EMERGENCY_MODE` on active mech | 64582 | detection signal (not a hazard) |
| persistent ground fire (`NPC_FLAMES_INITIAL` → `NPC_FLAMES_SPREAD`, aura `SPELL_FLAMES_AURA` 64561) | 34363 / 34121 | standing on a node burns; it spreads toward players all fight |
| VX-001 Frost Bomb (`NPC_FROST_BOMB`, explosion `64626` 10 / `65333` 25) | 34149 | large delayed AoE; leave its radius before detonation |
| Emergency Fire Bots (Water Spray `64619`) | 34147 | **friendly** — see below |

`SPELL_EMERGENCY_MODE (64582)` is applied to Leviathan MK II (`33432`) / VX-001 (`33651`)
/ Aerial Command Unit (`33670`) as each becomes active, and to P4 when all three reassemble.
Those mechs are always valid attack targets, so `HasAura(64582)` on any live mech is the
robust live "firefighter is active right now" signal (mirrors `FlameLeviathanActiveTowerMask`
testing the boss's empower auras).

### Ground fire
`NPC_FLAMES_INITIAL 34363` is dropped on random players and immediately spawns a
`NPC_FLAMES_SPREAD 34121`, both carrying `SPELL_FLAMES_AURA 64561`; the field creeps toward
the nearest player for the whole fight. Both are non-selectable trigger creatures (they
never appear in attack-target lists), so bots find them by scanning the `"nearest npcs"`
GuidVector — the same idiom as Freya's Unstable Sun Beam stalkers.

### Frost Bomb
VX-001 casts on a flame node → spawns `NPC_FROST_BOMB 34149` (visual aura `64624`), which
detonates in a large AoE (`SPELL_FROST_BOMB_EXPLOSION_10 64626` / `_25 65333`). Bots avoid
the bomb creature itself, so no spell id is referenced — leaving its radius before it blows
is sufficient, and works identically on 10 and 25.

### Emergency Fire Bots — correction to the master plan
The master plan listed "kill Emergency Bots" as a firefighter delta. Server truth: the
Emergency Fire Bots (`NPC_EMERGENCY_FIRE_BOT 34147`) are **friendly, non-combat fire
extinguishers**. They do not enter combat with players (`_option < 3` gates
`SetInCombatWithZone`; they are option 3), never heal or repair Mimiron, and only run to the
nearest flame node and cast Water Spray (`64619`) to put the fire OUT. They are **not** kill
targets — bots ignore them, and already would (never marked, never hostile).

### Heroic (10/25)
Both share identical NPC entries (difficulty is an instance property) and the same
`64582`/`64561` auras. Detection and both dodges key off NPC entries + `MoveAwayFromCreature`,
never a difficulty-specific spell cast, so normal and heroic are both covered with no spell-id
predicate.

---

## Scope implemented (mechanics only)

Matching the Freya precedent, bots only survive the added ground hazards — they do not
touch Mimiron's normal-mode kit (proximity mine, bomb bot, rocket strike, shock blast, laser
barrage, phase positioning, fire-resistance buff are all untouched):

1. **Dodge persistent ground fire** — a bot within `ULDUAR_MIMIRON_FLAMES_RADIUS` of a live
   `NPC_FLAMES_SPREAD`/`NPC_FLAMES_INITIAL` flees the node (`FleePosition`). `ACTION_RAID + 3`.
2. **Dodge Frost Bomb** — a bot within `ULDUAR_MIMIRON_FROST_BOMB_RADIUS` of `NPC_FROST_BOMB`
   moves clear before it detonates (`MoveAwayFromCreatureAction`). `ACTION_RAID + 3`.

Both dodges outrank the normal-mode Mimiron positioning actions (`ACTION_RAID`/`+2`), so a bot
standing in a hazard moves out first, then resumes its phase positioning once clear.

**Skipped:** the Emergency Fire Bots (friendly, above). No raid-wide undodgeable firefighter
damage exists to heal through beyond what normal mode already has.

The gate `IsMimironHardModeActive` = config-enabled AND an active mech carrying
`SPELL_EMERGENCY_MODE`. The fire/Frost-Bomb hazards only ever spawn in firefighter, so this
coarse gate plus the per-hazard check is sufficient and robust across all four phases.

## Files
`Util/UldBossHelper.h` (enums + radii), `Util/UldHardMode.{h,cpp}` (`IsMimironHardModeActive`),
`Trigger/UldTriggers_Mimiron.{h,cpp}`, `Action/UldActions_Mimiron.{h,cpp}`, `UldTriggerContext.h`,
`UldActionContext.h`, `UldStrategy.cpp`, `src/PlayerbotAIConfig.{h,cpp}`,
`conf/playerbots.conf.dist`.

## Verification
- `python apps/codestyle/codestyle-cpp.py` clean.
- Config toggle: `= 0` → bots ignore firefighter hazards (normal-mode behaviour unchanged);
  `= 1` → they react.
- In-game (local worldserver, bot raid + one human): human presses the Big Red Button, then
  confirm bots (a) flee flame nodes rather than standing in fire, (b) clear the Frost Bomb
  radius before detonation, (c) do not attack the friendly Emergency Fire Bots, and (d) resume
  normal positioning once clear. With config on but firefighter NOT triggered, the two dodges
  stay inactive.
