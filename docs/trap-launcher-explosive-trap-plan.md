# Plan: Hunter bots use Trap Launcher: Explosive Trap in DPS rotation

## Context

`mod-spell-tweaks` added a new Hunter ability **Trap Launcher: Explosive Trap** — 6 per-rank
launcher spells `425777`–`425782`, each a ground-targeted (0–40 yd, `Targets=64` DEST_LOCATION)
clone of the matching Explosive Trap rank that summons that rank's trap gameobject at the
clicked location. Launchers are auto-learned when a hunter trains the matching Explosive Trap
rank, share the 30 s trap cooldown (`Category=1250`, `RecoveryTime=30000`), cost 19% base mana,
and all share the spell name `"Trap Launcher: Explosive Trap"` with `Rank N` subtext.

Today the playerbot AI only ever drops Explosive Trap **at the bot's feet**, via the
`enemy within melee` trigger (`GenericHunterStrategy.cpp:79`, prio 37.0) — i.e. only when a mob
is already in melee. The bots never use the trap as ranged DPS. Goal: make **all three DPS specs
(BM, MM, Survival)** launch Explosive Trap at their target **on cooldown** as a ranged rotation
filler.

## Why this is a small change

- The bot's **unit-targeted** cast already handles DEST_LOCATION spells: `PlayerbotAI::CastSpell(id, Unit*)`
  sets `targets.SetDst(*target)` for `TARGET_FLAG_DEST_LOCATION` (`PlayerbotAI.cpp:3677-3682`).
  So casting the launcher at the current enemy drops the trap on that enemy — no x/y/z plumbing.
- Name→spell-id resolution picks the **highest known rank** by parsing the `Rank` subtext
  (`SpellIdValue.cpp:96-163`); the rank-6 row confirms subtext `"Rank 6"`. So casting by name
  auto-selects the correct rank for the bot's level.
- Precedent: **Volley** (a dest/AoE spell) is already cast via a plain `CastSpellAction`
  (`HunterActions.h:502`, `CastVolleyAction`). The launcher mirrors this exactly.
- Cast-on-cooldown trigger already exists: `SpellNoCooldownTrigger` fires when the named spell is
  known and off cooldown (`GenericTriggers.cpp:305-312`), as used by `ImmolationTrapNoCdTrigger`.

No SQL and no hardcoded spell IDs in mod-playerbots — everything keys off the spell name.

## Changes (all in mod-playerbots `src/Ai/Class/Hunter/`)

### 1. New action — `HunterActions.h`
Add next to `CastExplosiveTrapAction` (~line 361), mirroring it but with the launcher name:

```cpp
class CastTrapLauncherExplosiveAction : public CastSpellAction
{
public:
    CastTrapLauncherExplosiveAction(PlayerbotAI* botAI) :
        CastSpellAction(botAI, "trap launcher: explosive trap") {}
};
```

The inherited `CastSpellAction::Execute` (`GenericSpellActions.cpp:180`) casts at the current
target; the DEST_LOCATION handling drops the trap on that target. No override needed.
(The name string must be exactly `"trap launcher: explosive trap"` — `SpellIdValue` matches the
full spell name including the colon.)

### 2. New trigger — `HunterTriggers.h`
Add next to `ImmolationTrapNoCdTrigger` (~line 224):

```cpp
class TrapLauncherExplosiveNoCdTrigger : public SpellNoCooldownTrigger
{
public:
    TrapLauncherExplosiveNoCdTrigger(PlayerbotAI* botAI)
        : SpellNoCooldownTrigger(botAI, "trap launcher: explosive trap") {}
};
```

Fires only when the launcher is known (spell id resolves non-zero → hunter has trained an
Explosive Trap rank) and off cooldown. Since it shares the trap cooldown, this is inactive while
Explosive Trap is on CD.

### 3. Register action + trigger — `HunterAiObjectContext.cpp`
- In the action creator map (with the other trap actions, ~line 192/249): register
  `"trap launcher: explosive trap"` → `new CastTrapLauncherExplosiveAction(botAI)`.
- In the trigger creator map (with `"immolation trap no cd"`, ~line 92/130): register
  `"trap launcher: explosive trap no cd"` → `new TrapLauncherExplosiveNoCdTrigger(botAI)`.

Match the existing `#include` and factory patterns already in the file.

### 4. Wire into rotation — `GenericHunterStrategy.cpp` (`InitTriggers`)
Add one trigger node in the base `GenericHunterStrategy::InitTriggers` (shared by BM/MM/Survival),
in the "Ranged-based Triggers" section (~line 78):

```cpp
triggers.push_back(new TriggerNode("trap launcher: explosive trap no cd",
                                   { NextAction("trap launcher: explosive trap", 17.0f) }));
```

**Priority 17.0** rationale: sits just below Survival's Explosive Shot (17.5) and above Black
Arrow (16.5) and serpent-sting fillers; for BM/MM it lands below Kill Command (18.5) / Kill Shot
(18.0) / Viper Sting (17.5) and comfortably above auto shot — a proper ranged filler in every
spec. It naturally yields to the melee `enemy within melee` → explosive trap (37.0) when a mob is
on top of the bot; both share the cooldown so they never double-fire.

## Notes / decisions
- Leave the existing melee `enemy within melee` → explosive-trap feet-drop as-is (harmless; higher
  priority handles the mob-in-melee case, launcher handles the ranged case).
- No AoE-pull suppression guard (unlike Starfall/Volley) — this matches the existing Explosive
  Trap action's behavior and keeps parity; can be added later if bots pull neighbors.
- Header-only: no new `.cpp` bodies required.

## Verification
1. Build worldserver (only when ready): the four edits are the only changes.
2. In-game: spawn/possess a Hunter bot ≥ level 34 that knows Explosive Trap (launcher auto-learned
   on login via `spell_tweaks_hunter_trap_launcher_learn`). Confirm it knows
   `Trap Launcher: Explosive Trap`.
3. Enter combat with a ranged target. Expect the bot to cast **Trap Launcher: Explosive Trap** at
   the target's location roughly every 30 s (green trap lands on the mob, not at the bot's feet),
   interleaved with its normal shots.
4. Confirm rank scaling: a level-77 bot casts rank 6 (`425782`); lower-level bots cast the highest
   rank they know.
5. Optional: enable the `debug spell` strategy to log the resolved spell id and confirm the
   correct launcher rank fires and respects the shared 30 s cooldown.
