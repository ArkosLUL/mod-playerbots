# EoE P1 — push the tank spot out, stop off-tanks ripping Malygos, fix the rogue redirect

## Context

Three phase-1 observations from in-game play, in the mod-playerbots Eye of Eternity strategy
(`modules/mod-playerbots/src/Ai/Raid/EoE/`, map 616, Malygos):

1. The main tank holds too close to the middle of the arena.
2. The off-tank sometimes rips Malygos off the main tank, dragging him inward into the melee stack
   and sweeping the Arcane Breath cone (56272, a frontal cone on his *current victim*) through the
   raid.
3. Open question, now answered: do hunters and rogues redirect threat onto the main tank in P1?

All three are P1-only. Nothing in P2/P3 changes.

### What the platform actually looks like

`MALYGOS_MAINTANK_OFFSET` cannot be raised blind — the tank spot is a signed distance from
`CenterPos` and past the floor edge the bot falls off. The usual check does not work here: map 616
ships **0 mmtiles and no vmap tree**, and `navprobe --map 616 coverage` reports terrain flat at 0.0
everywhere, so it cannot answer where the floor ends.

The floor was measured instead, from the platform's collision model. `gameobject` on map 616 holds
`Nexus Raid Platform` (entry 193070, displayId 8387) at `{754.35, 1300.87, 256.25}`; the display
entry names `Nexus_Raid_Floating_Platform.wmo`, whose extracted collision mesh lives in the
`ac-client-data` Docker volume at `vmaps/Nexus_Raid_Floating_Platform.wmo.vmo`. Parsing its
`GMOD`/`VERT` chunks gives a **stepped** floor, three tiers (model Z plus the 256.25 spawn Z):

| world Z | radius band from centre |
|---|---|
| 266.10 | 0 → **29.0** — inner disc, this is `CenterPos.z` |
| 267.25 | 30.5 → **47.5** — middle ring |
| 268.25 | 48.1 → **55.5** — outer rim, the true edge |

Cross-check that validates the mapping: the `Exit Portal` GO spawns at r **43.41**, z **267.23** —
on the middle-ring tier, within 0.02 yd. The current tank spot at +42 sits on that same ring.

### What is actually wrong with the off-tank

`MalygosTargetAction` (`EoEActions.cpp:817`) puts the off-tank on Malygos in P1: the Power Spark
peel is gated on `botAI->IsDps(bot)`, which a tank-spec bot fails, so `newTarget` stays the boss.
`MalygosPositionAction` then parks him on the melee stack at +12. He melees Malygos from ~8.5 yd.

`MalygosMultiplier`'s P1 `cast` branch (`EoEMultipliers.cpp:63-79`) zeroes only
`CastBlinkBackAction`, `CastDisengageAction` and non-boss-tank `CastReachTargetSpellAction`. **No
taunt is gated at all.**

There are two taunt paths, and they are not equally guarded:

- **Single-target** — the class tank strategies wire `lose aggro` → `taunt` (warrior,
  `ACTION_INTERRUPT + 1`), `hand of reckoning` (paladin, `ACTION_HIGH + 7`), `dark command` (DK,
  `ACTION_HIGH + 3`), `growl` (bear druid, 26.0). `LoseAggroTrigger` is `!has aggro on current
  target`, and `HasAggroValue::Calculate` (`src/Ai/Base/Value/AttackerCountValues.cpp:13`) already
  answers "has aggro" whenever the victim is *another tank player* and the bot is not the explicitly
  flagged main tank. `IsExplicitMainTank` implies `IsMainTank` (`GetMainTankGuid` checks
  `MEMBER_FLAG_MAINTANK` first), so between two tank bots this path is mostly already quiet. It
  opens when Malygos' victim is **not** a tank player — the pull scramble, a pet, a threat spike.
- **AoE** — `high aoe` → `challenging shout` (warrior, `ACTION_HIGH + 3`) and `challenging roar`
  (bear druid, 26.5). `HighAoeTrigger` is 4 alive attackers within 8 yd of the current target.
  **Nothing gates these on who is tanking.** Challenging Shout (1161) is a radius taunt around the
  caster and the off-tank stands 8.5–12.5 yd from Malygos, so it can land on the boss. This path is
  completely ungated today.

Every taunt action derives from `CastSpellAction` (directly, or via `CastMeleeSpellAction`), so the
multiplier's existing `move ? nullptr : dynamic_cast<CastSpellAction*>(action)` shortcut already
catches all of them — no new dynamic_cast family is needed.

### Hunters and rogues — the answer, and the one thing worth fixing

- **Hunters redirect, and it works.** `GenericHunterStrategy` wires `low tank threat` →
  `misdirection on main tank` at `ACTION_HIGH + 7`, plus a second node on `misdirection on main tank
  and light aoe`. `BuffOnMainTankAction` targets the `main tank` value. Misdirection (34477) has a
  **100 yd** range and the hunter spot is 56 yd from the tank spot, so range is never the limit.
  Nothing EoE-specific suppresses it. One wart, left alone: when the hunter's current target is a
  Power Spark the tank has 0 threat on it, so `LowTankThreatTrigger` fires and the redirect is spent
  moving spark threat.
- **Rogues try every tick and never land it.** `TricksOfTheTradeTargetValue::Calculate`
  (`src/Ai/Class/Rogue/RogueValues.cpp:32`) returns the main tank whenever `TankNeedsRedirect` holds
  — the 10 s opener, or the rogue above 50 % of tank threat — **with no range check on that
  branch**; the 20 yd `REDIRECT_RANGE` only gates the melee-DPS fallback below it. Tricks of the
  Trade (57934) is a **20 yd** spell, and the rogue sits at the melee stack 30 yd from the tank spot
  (34 yd after this change). `PlayerbotAI::CanCastSpell` deliberately treats
  `SPELL_FAILED_OUT_OF_RANGE` as castable (`PlayerbotAI.cpp:3446`), so the action is attempted;
  `CastSpell` then fails on `spell->prepare` and returns false (`PlayerbotAI.cpp:3817`). Net effect:
  a `Spell` allocated and thrown away every tick, and the tank never gets the redirect, until the
  opener lapses and the fallback picks a melee DPS within 20 yd — which does work.

## Approach

### 1. Push the main tank spot out — 42 → 46 yd

`src/Ai/Raid/EoE/EoEActions.h:32`:

```cpp
const float MALYGOS_MAINTANK_OFFSET = 46.0f;
```

One constant covers all four landing bearings: `GetMalygosP1Layout` rotates the same signed offsets
onto whichever of `MALYGOS_LANDING_ANGLES` Malygos is nearest, so "all layouts" needs no per-bearing
work. Only `EoEActions.cpp:295` reads it.

46 is the largest value that keeps every existing P1 invariant intact. Malygos parks ~21.5 yd short
of his victim (CombatReach 20), so the whole geometry shifts out with the tank:

| check | at 46 | limit |
|---|---|---|
| ring margin — tank to the 47.5 step | 1.5 yd | must stay on the middle ring |
| tank ← → healers on the +12 stack | 34.0 yd | `HealDistance` **38.5** (conf default, no `AC_` override) |
| Malygos ← → melee stack | 12.5 yd | `MALYGOS_MELEE_HOLD_DISTANCE` 15, above which the stack spot is clamped inward |
| Malygos ← → DK grip spot at −1 | 25.5 yd | `POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE` 18 minimum |
| hunters at −14 ← → Malygos | 37.5 yd | above the ~28 yd inflated minimum, far below `SpellDistance` 28.5 **+ his 20 CombatReach** |

Do not go past 47.5 (the ring step), and note the melee clamp starts firing past 48.5 anyway.

### 2. Gate P1 taunts on whether a tank already holds Malygos

Chosen behaviour: **an off-tank may not taunt Malygos while a tank player is holding him**; when
nobody with a tank spec has him — the pull scramble, a pet holding him, the main tank dead — the
taunt is still allowed, so the rescue path survives. This deliberately leaves the pull-scramble race
open.

`src/Ai/Raid/EoE/EoEMultipliers.h` — add one snapshot field beside `isBossVictim`:

```cpp
bool bossVictimIsTank = false;
```

`src/Ai/Raid/EoE/EoEMultipliers.cpp`, in `RefreshSnapshot` (already 500 ms cached, which is what
makes the `IsTank` strategy walk affordable):

```cpp
Player* victim = boss ? (boss->GetVictim() ? boss->GetVictim()->ToPlayer() : nullptr) : nullptr;
bossVictimIsTank = victim && botAI->IsTank(victim);
```

In the phase-1 `cast` branch, before the existing returns:

```cpp
// Only the assigned tank pulls Malygos back. Arcane Breath is a frontal cone on his current
// victim, so an off-tank taunting from the melee stack turns the boss inward and sweeps the raid.
// Still allowed when no tank has him at all - a pet holding him, or the main tank dead.
if (!isMainTank && bossVictimIsTank && IsMalygosTauntAction(cast->getName()))
{
    return 0.0f;
}
```

`IsMalygosTauntAction` is a file-local helper over a static name set — `"taunt"`,
`"hand of reckoning"`, `"dark command"`, `"growl"`, `"challenging shout"`, `"challenging roar"`.
Names rather than `dynamic_cast` because the alternative pulls four class action headers into the
EoE multiplier for six classes, and comparing `action->getName()` is established practice in this
tree (`NaxxMultipliers.cpp:175`, `NaxxMultipliers.cpp:564`, `UldMultipliers.cpp:212`).

Do **not** also suppress `"heroic throw"`. It is the warrior `taunt` node's alternative, so zeroing
`taunt` re-pushes it at 0.003 relevance (`Engine.cpp:238` pushes alternatives with
`forceRelevance = relevance + 0.003f` when a multiplier zeroes the action) — but Heroic Throw is
damage, not a taunt, and at that relevance it loses to everything.

### 3. Range-check the rogue's main-tank redirect

`src/Ai/Class/Rogue/RogueValues.cpp:39` — apply the same `REDIRECT_RANGE` the fallback already uses:

```cpp
if (mainTank && mainTank != bot && bot->GetDistance(mainTank) <= REDIRECT_RANGE &&
    TankNeedsRedirect(mainTank))
    return mainTank;
```

Shared rogue code, not EoE-specific: every fight where the tank stands more than 20 yd out currently
burns the `ACTION_HIGH + 6` slot on a cast that cannot succeed. With the check the value falls
straight through to the hardest-hitting melee DPS in range, which is the intended second half of the
rule the class comment already describes.

### 4. Docs

Run `/compact-docs-writer` **before** editing, not as cleanup after.

`modules/mod-playerbots/docs/raids/eye-of-eternity.md`:

- The `MALYGOS_MAINTANK_OFFSET` bullet (line 149) says **+42** and justifies it from the Exit Portal
  alone. Replace with **+46** and the measured three-tier floor table, keeping the portal as the
  cross-check rather than the sole evidence, and record that navprobe cannot answer this map.
- Add the taunt gate to the P1 section: both taunt paths, that `HasAggroValue` already covers most
  of the single-target one while the AoE one was ungated, and that the rescue case is deliberately
  left open.
- Add the hunter/rogue redirect facts — Misdirection 100 yd works, Tricks 20 yd did not, and the
  `RogueValues` fix.

## Files

| File | Change |
|---|---|
| `src/Ai/Raid/EoE/EoEActions.h` | `MALYGOS_MAINTANK_OFFSET` 42 → 46, comment updated with the ring bound |
| `src/Ai/Raid/EoE/EoEMultipliers.h` | `bossVictimIsTank` snapshot field |
| `src/Ai/Raid/EoE/EoEMultipliers.cpp` | snapshot fill, taunt name helper, P1 cast-branch gate |
| `src/Ai/Class/Rogue/RogueValues.cpp` | range check on the main-tank branch |
| `docs/raids/eye-of-eternity.md` | tank offset bullet, taunt gate, redirect facts |

No new files, no wiring sites — `EoEStrategy.cpp`, the trigger/action contexts and `EoETriggers`
are untouched.

## Verification

**Static** — the module cannot be compiled in this environment, so this is the ceiling here:

- `python apps/codestyle/codestyle-cpp.py` from the server root must pass (blocking CI)
- `cppcheck` must produce an empty report (blocking CI)
- confirm the six taunt action names still resolve — `grep -rn '"taunt"\|"hand of reckoning"\|"dark
  command"\|"growl"\|"challenging shout"\|"challenging roar"' src/Ai/Class/*/[A-Z]*AiObjectContext.cpp`

**In game**, P1 on any of the four landing bearings:

1. **Tank spot.** The main tank should hold visibly further out, with Malygos parked ~24.5 yd from
   centre. Nobody falls off; healers keep the tank topped without walking out of the stack. Watch
   one pull on each bearing — the layout is latched per pull, so a single pull only exercises one.
2. **Off-tank.** With the main tank alive and holding, the off-tank must never move Malygos. Melee
   from the stack as before.
3. **Rescue path still open.** Kill or pull the main tank away and Malygos should still get picked
   up by the off-tank rather than eating the raid.
4. **Melee stack unclamped.** The stack spot should sit where the layout puts it at +12; if bots
   creep outward towards the boss, the 15 yd clamp is firing and the offset is too large.
5. **Rogues.** No more failed Tricks casts at the pull; the buff should land on the top melee DPS at
   the stack instead. `debug spell` strategy shows the cast failures if any remain.
