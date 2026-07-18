# Algalon the Observer — Playerbot Raid Strategy

## Context

mod-playerbots has a single `RaidUlduarStrategy` (`getName() == "ulduar"`, map 603) that already
serves every Ulduar boss (Flame Leviathan … Yogg-Saron) via per-mechanic trigger→action pairs.
**Algalon the Observer is the only Ulduar encounter with zero playerbot coverage** — grep for
`algalon` across `src/Ai/Raid/Uld/` returns nothing. Goal: add Algalon mechanic handling so bots
survive and clear the fight, following the existing repeatable Uld pattern (no new strategy, no
engine wiring — the context/strategy/map-id are already registered).

Core AC script: `src/server/scripts/Northrend/Ulduar/Ulduar/boss_algalon_the_observer.cpp`.

## Encounter findings (mechanics that matter to bots)

Boss creature name for `AI_VALUE2(Unit*, "find target", ...)` → `"algalon the observer"`
(verify against `creature_template.name`).

| Mechanic | IDs | Bot-relevant behaviour |
|---|---|---|
| Cosmic Smash | selector `62301`, impact `62304`, telegraph `62300`; target NPCs `33104`/`33105` | Ground meteors (1 in 10-man, 3 in 25). Damage falls off past ~10 yd. **Dodge ≥10 yd from each asteroid-target NPC.** |
| Big Bang | `64443`, cast every 90.5s, ~long cast | Raid-wide lethal to anyone **not** phased. Entering a Black Hole grants phase aura (`62169`) → safe. **See evade caveat below.** |
| Phase Punch | `64412` (stacks 1→5) | Tank debuff; at 5 stacks tank is fully phased out. **Tank swap before 5.** |
| Black Hole chain | Collapsing Star `32955` → Black Hole `32953`; P2 Worm Hole `34099` | Black Holes are the Big Bang shelter and the constellation sink. |
| Living Constellation | `33052`, phase-effect `65509` | Cast Arcane Barrage; must be **led onto a live Black Hole** — contact despawns both. |
| Unleashed Dark Matter | `34097` (P2, from Worm Holes) | Chases a random player; focus-kill. |
| Quantum Strike | `64395` | Melee tank strike (no bot action needed). |
| Ascend/enrage | `64487` at 6 min | Hard DPS check; also fires on the Big Bang evade. |

**Difficulty:** Algalon has no separate heroic script — 10N and 25N share one AI; NPC entries and
spell ids are identical, only the Cosmic Smash meteor count differs (1 vs 3), which is irrelevant
since bots dodge every asteroid-target NPC present. So the "both normal and heroic" requirement is
satisfied with **no** 4-id spell predicate — logic is fully difficulty-agnostic.

**Big Bang evade caveat (user chose "hide everyone"):** `spell_algalon_big_bang::CheckTargets`
(`boss_algalon_the_observer.cpp:1260-1265`) calls `ACTION_ASCEND` (boss evades/resets) when the
Big Bang hits **zero** targets. If a pure-bot raid all hides, the boss resets. Per the user's
decision we implement all bots hiding (lore-accurate); the fight then requires **at least one
non-bot player to soak Big Bang** (or accept the reset). Documented here, not worked around.

## Approach (per user decisions)

- **Big Bang:** every bot runs into the nearest Black Hole / Worm Hole for the cast.
- **Constellations:** designated kiter leads each Living Constellation onto a live Black Hole.
- **Phase Punch:** two tanks alternate taunt when the active tank's stacks get high.

## Implementation — files touched (all existing; no new files, no engine/wiring changes)

Add Algalon ids to `enum UlduarIDs` in **`src/Ai/Raid/Uld/Util/UldBossHelper.h`**:
`NPC_ALGALON=32871`, `NPC_LIVING_CONSTELLATION=33052`, `NPC_COLLAPSING_STAR=32955`,
`NPC_BLACK_HOLE=32953`, `NPC_WORM_HOLE=34099`, `NPC_UNLEASHED_DARK_MATTER=34097`,
`NPC_ALGALON_ASTEROID_TARGET_1=33104`, `NPC_ALGALON_ASTEROID_TARGET_2=33105`,
`SPELL_ALGALON_BIG_BANG=64443`, `SPELL_ALGALON_PHASE_PUNCH=64412`,
`SPELL_ALGALON_COSMIC_SMASH=62301`, `SPELL_ALGALON_BLACK_HOLE_DAMAGE=62169`.

Then add, following the existing Hodir/Freya/Kologarn patterns exactly:

### Triggers — `UldTriggers.{h,cpp}` + register in `UldTriggerContext.h`
Each `IsActive()` first gates on `AI_VALUE2(Unit*, "find target", "algalon the observer")` alive.

1. **`algalon cosmic smash trigger`** — nearest `33104`/`33105` within ~11 yd (model:
   `FreyaNearNatureBombTrigger`, `UldTriggers.cpp:570`).
2. **`algalon big bang trigger`** — boss `FindCurrentSpellBySpellId(SPELL_ALGALON_BIG_BANG)` AND
   bot lacks the black-hole phase aura `62169` (model: `HodirNearSnowpackedIcicleTrigger`,
   `:541`, which keys off the boss cast).
3. **`algalon phase punch trigger`** — the current boss victim (a tank) has `64412` at stack
   ≥ threshold (e.g. 3) and this bot is the designated off-tank.
4. **`algalon constellation kite trigger`** — a live `33052` exists AND a live `Black Hole 32953`
   exists AND this bot is the designated kiter (`IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID)`
   — first alive DPS; helper already in `RaidBossHelpers`).
5. **`algalon dark matter trigger`** — a live `34097` exists (P2 focus).

### Actions — `UldActions.{h,cpp}` + register in `UldActionContext.h`
Each `isUseful()` re-checks its trigger (existing convention).

1. **`algalon cosmic smash action`** (`MovementAction`) — `FleePosition(nearestAsteroid->GetPosition(),
   12.0f)` (model: `FreyaMoveAwayNatureBombAction`, `UldActions.cpp:1506`). Priority
   `ACTION_EMERGENCY`.
2. **`algalon big bang hide action`** (`MovementAction`) — `MoveTo` nearest `32953`/`34099` (model:
   `HodirMoveSnowpackedIcicleAction`, `:1440`). Priority `ACTION_EMERGENCY + 1` (lethal).
3. **`algalon phase punch swap action`** (`Action`) — off-tank taunts the boss (cast class taunt on
   boss / set as current target) so the active tank's stacks decay. Gate on
   `botAI->IsAssistTankOfIndex` / `IsMainTank` + `GetGroupMainTank`/`GetGroupAssistTank`
   (`RaidBossHelpers`). Priority `ACTION_RAID + 2`.
4. **`algalon constellation kite action`** (`MovementAction`) — kiter takes/holds the nearest
   `33052` and `MoveTo` the nearest live Black Hole so the add is dragged into `65509` (both
   despawn). Priority `ACTION_RAID + 1`.
5. **`algalon dark matter mark action`** (`AttackAction`) — mark `34097` with skull +
   `SetRtiTarget(botAI, "skull", darkMatter)` so the group focus-kills it (model:
   `FreyaMarkDpsTargetAction`, `:1521`). Priority `ACTION_RAID`.

### Strategy wiring — `UldStrategy.cpp`
Append an `// Algalon` banner block in `InitTriggers` with one `triggers.push_back(new TriggerNode(
"<trigger>", { NextAction("<action>", <priority>) }))` per pair above (model: the Hodir/Freya
blocks at `:115-152`).

### No changes needed
- `BuildSharedTriggerContexts.cpp:49` / `BuildSharedActionContexts.cpp:49` already `Add(new
  RaidUlduar*Context())`.
- `RaidStrategyContext.h:47,71` already registers `"ulduar"`.
- Map-id 603 auto-activation already wired (Ulduar strategy is live) — **verify** the `case 603`
  and allowed-list entry exist in `PlayerbotAI.cpp`; add only if missing.
- No `UldMultipliers` (this strategy has none) and no per-difficulty predicate.

## Verification

Do **not** build unless asked (slow C++). When building is authorised:
1. `python apps/codestyle/codestyle-cpp.py` — style must pass (4-space, Allman, `auto const&`,
   `Type const*`, `{}` fmt).
2. Build worldserver; in-game spawn Algalon (`.go` / GM start), fill group with bots + the
   `"ulduar"` strategy auto-applied on map 603.
3. Observe per mechanic: bots flee asteroid impact zones (Cosmic Smash); bots pile into a Black
   Hole on Big Bang cast and survive (with a human soaker present, boss does not evade); off-tank
   taunts before the active tank hits 5 Phase Punch stacks; kiter drags Living Constellations onto
   Black Holes (both despawn); in P2, group focuses Unleashed Dark Matter.
4. Confirm the shared strategy stays inert during other Ulduar bosses (all triggers gate on the
   Algalon `find target`).

## Implementation notes (deviations from plan, as built)

- **Boss `find target` qualifier is `"algalon the observer"`** (full lowercase creature name),
  not `"algalon observer"`. `FindTargetValue::Calculate` requires an *exact-length*
  case-insensitive match against `creature_template.name` (`Algalon the Observer`, entry 32871),
  and only scans the bot's threat list — so every gate needs the bot in combat with the boss.
- **Constellation kite is movement-only.** `MovementAction` exposes `MoveTo`/`FleePosition` but not
  `Attack` (that lives on `AttackAction`; the two are siblings under `Action`). The kiter
  (`IsMechanicTrackerBot`) stands on the nearest live Black Hole (`32953`) while an *active*
  (selectable) Living Constellation exists, dragging the chasing add through the phase field. No
  forced taunt on the add (constellations aren't reliably tauntable by bots anyway).
- **Phase Punch swap** action is an `AttackAction` (needs `Attack` + `taunt spell`); trigger fires
  when the boss's current victim reaches `ULDUAR_ALGALON_PHASE_PUNCH_SWAP_STACKS` (3) and the
  first assist tank's own stacks have decayed below that.
- Difficulty-agnostic for free: Algalon's Big Bang / Phase Punch / Cosmic Smash / Black Hole aura
  ids are shared across 10/25 (Ulduar has no separate heroic), so no 4-id predicate was needed.
