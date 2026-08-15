# XT-002 Deconstructor: anchored positioning + RTI-free targeting

## Context

Our XT-002 strategy (already merged on `Custom`) is behaviourally richer than upstream PR
[mod-playerbots#2653](https://github.com/mod-playerbots/mod-playerbots/pull/2653) — it covers hard
mode, the Heart HP safety floor, Boombot avoidance, threat redirects and burst-window gating, none of
which the PR has. But it is entirely *emergent*: every XT-002 movement is an 8-direction ring search
(`XT002MoveClearAction::MoveClearOf`) and there is not a single fixed coordinate anywhere in it. The
PR's one real advantage is that it *anchors* the fight — a main tank spot, a ranged spot, a designated
Searing Light spot, and a Gravity Bomb parking grid that packs hard-mode Void Zones into one corner
instead of scattering them across the room.

Two changes:

1. **Adopt the PR's anchoring** — fixed spots for the main tank, ranged DPS and the Searing Light
   carrier, plus the Gravity Bomb parking grid.
2. **Drop RTI entirely** — the current design marks a skull and routes bots through the generic
   `attack rti target`. Raid icons are group-global, so XT-002 stamps over marks other bots and the
   player rely on. Replace with the Sunwell pattern: one self-resolving priority action calling
   `Attack()` directly, plus a multiplier standing the generic targeting down.

Net node count after the change: **−1 trigger, ±0 actions**. No complexity growth over what is merged.

## Established facts

Verified this session — do not re-derive.

| Fact | Source |
|---|---|
| XT-002 spawns at `(886.28, -12.05, 409.6)`, orientation `3.13` (faces −x) | `acore_world`.`creature`, entry 33293 |
| Void Zone **and** Life Spark both spawn only when `xt002->HasAura(Heartbreak)` — the real aura, not any config | [boss_xt002.cpp:747](../../../src/server/scripts/Northrend/Ulduar/Ulduar/boss_xt002.cpp#L747), [:772](../../../src/server/scripts/Northrend/Ulduar/Ulduar/boss_xt002.cpp#L772) |
| Searing Light and Gravity Bomb each repeat every 16s (25man) / 20s (10man) — longer than the debuff, so never two carriers of the same type at once | [boss_xt002.cpp:367-374](../../../src/server/scripts/Northrend/Ulduar/Ulduar/boss_xt002.cpp#L367-L374) |
| `Engine::DoNextAction` **breaks on the first action that returns true** — relative node priority starves, it does not merely order | [Engine.cpp:219-225](src/Bot/Engine/Engine.cpp#L219-L225) |
| `PlayerbotAI::IsRanged(p)` (default `bySpec=false`) returns `ContainsStrategy(STRATEGY_TYPE_RANGED)` — **healers count as ranged** | [PlayerbotAI.cpp:1905](src/Bot/PlayerbotAI.cpp#L1905) |
| `PlayerbotAI::IsRangedDps(p)` exists = `IsRanged && IsDps` | [PlayerbotAI.h:460](src/Bot/PlayerbotAI.h#L460) |
| `nearest npcs` uses `sightDistance` (default 100yd) — covers the whole XT-002 room | [NearestNpcsValue.h:18](src/Ai/Base/Value/NearestNpcsValue.h#L18) |
| `possible targets` rejects `UNIT_FLAG_NOT_SELECTABLE`, which is why the unexposed Heart never appears | [AttackersValue.cpp:155](src/Ai/Base/Value/AttackersValue.cpp#L155) |

Only XT himself is DB-spawned in that room; every add is script-summoned, so room geometry cannot be
bounded from the DB. Handled by Q15 below rather than by testing.

## Design decisions

Settled during review. Each carries its reasoning because several are non-obvious.

1. **Parking gates on `IsXT002HeartbreakActive`**, not `IsXT002HardModeActive`. The latter is
   config-only ([UldHardMode.h:74](src/Ai/Raid/Uld/Util/UldHardMode.h#L74)); the former reads the same
   aura the core script checks, so parking switches on exactly when Void Zones start existing.
2. **Anchor `IsRangedDps` only.** Healers keep default positioning — it tracks heal range, which a
   fixed point cannot, and `RANGED_SPOT` would otherwise sit ~30yd from the tank with no slack.
3. **`RANGED_SPOT` moves in to `(866.0, -12.5, 409.8)`.** The tank stands *on* `MAINTANK_SPOT`, so XT
   settles ~890; the PR's 858.4 leaves untalented casters at ~32yd, clipping their 30yd range and
   making them walk in every tick. 866 puts them ~24yd out and still 25.5yd from the Searing Light
   spot, clear of the 12yd spread radius.
4. **Gather targets from `nearest npcs`**, not SWP's `possible targets no los`. Every existing XT-002
   trigger already forces that value, so the priority action adds zero new grid work.
5. **Leave `attack rti target` registered and live.** We stop *setting* marks; we do not stop obeying
   them. A human marking a Scrapbot is a deliberate instruction and should win.
6. **`MOVEMENT_COMBAT`, no `HasCheat` teleport branch.** The Yogg-Saron phase-3 action uses
   `MOVEMENT_FORCED` + teleport because that phase is a platform transition; XT-002 is one flat room,
   and `MOVEMENT_FORCED` would outrank the emergency dodges — the one thing that must never happen.
7. **Suppress generic movers, scoped to `IsRangedDps`.** Without it the anchor oscillates across ticks:
   bot reaches the spot, trigger goes false, a generic mover walks it back, trigger fires again. Same
   failure `RazorscaleMultiplier` exists to prevent ([UldMultipliers.h:63](src/Ai/Raid/Uld/UldMultipliers.h#L63)).
8. **No melee anchor.** Melee already have Boombot avoidance and the spread actions; pinning them costs
   uptime on a boss that moves.
9. **`raid position` stays the lowest node and yields to add control.** Because the engine breaks on
   first success, the priority action starves position during add churn. That is correct — killing a
   Scrapbot beats standing on a dot — and the 5yd tolerance makes the drift cheap. Comment it at the
   trigger node so it reads as deliberate.
10. **Exempt our own movers via a name allowlist**, not a marker base class. No single base class
    identifies "ours" (`MoveAwayFromCreatureAction` vs `XT002MoveClearAction` vs plain `MovementAction`),
    and the multiplier already carries a name set.
11. **Sticky same-entry tie-break measures distance to the bot**, not to a fixed anchor. Muru uses
    `MURU_STACK_POSITION` because its raid is stacked and adds path inward; XT-002's adds come from toy
    piles on both flanks.
12. **The Heart gate stays on the config flag.** It encodes *intent* — whether this raid means to go
    for the achievement — and must be known before Heartbreak exists, since breaking the Heart is what
    creates it. Reading the aura there would be circular.
13. **No fallback when no bot is main tank.** If a human tanks, the human decides where the boss goes.
    Ranged still anchor; if the human tanks elsewhere they drift out of range and generic movers take
    over — degraded, not broken.

## Reference: the Sunwell targeting pattern

Copy the shape of `MuruSetDpsPriorityAction`,
[SWPActions_Muru.cpp:178-393](src/Ai/Raid/SWP/Action/SWPActions_Muru.cpp#L178-L393):

- One pass over the unit list building a role-dependent `vector<pair<entry, Unit*>>` priority list,
  filtered through an `isAllowedPriorityTarget` lambda.
- **Sticky per entry** (`SelectMuruEncounterTarget`): keep the current target if it is the same entry,
  switch only when another candidate is >10yd closer. Stops two-Scrapbot ping-pong.
- **Sticky across entries** (`getPriorityIndex`): keep the current target when its priority index is
  `<=` the desired one.
- `needsAttack` guard so the action returns `false` when already on the right target — this is what
  lets lower nodes run at all, given the engine breaks on first success.
- Fall back to `AI_VALUE(Unit*, "dps target")`.
- Stand the default targeting down with a multiplier zeroing `DpsAssistAction`
  ([SWPMultipliers.cpp:712](src/Ai/Raid/SWP/SWPMultipliers.cpp#L712)).

## Files to change

### 1. `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp`

Add five `Position` constants (extern in the header, defined beside the other Ulduar positions):

```cpp
const Position ULDUAR_XT002_MAINTANK_SPOT              = Position(895.82f,    -12.53954f, 409.68756f);
const Position ULDUAR_XT002_RANGED_SPOT                = Position(866.0f,     -12.5f,     409.8f);
const Position ULDUAR_XT002_SEARING_LIGHT_SPOT         = Position(862.73724f,  12.77857f, 409.8322f);
const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_MELEE  = Position(871.5199f,  -54.04216f, 409.80377f);
const Position ULDUAR_XT002_GRAVITY_BOMB_ORIGIN_RANGED = Position(837.0746f,  -53.01061f, 409.80362f);
```

All but `RANGED_SPOT` are the PR's values. Add tolerances and grid geometry beside the existing
`ULDUAR_XT002_*` floats (~line 378):

```cpp
constexpr float ULDUAR_XT002_MAINTANK_SPOT_TOLERANCE = 3.0f;
constexpr float ULDUAR_XT002_RANGED_SPOT_TOLERANCE   = 5.0f;
constexpr float ULDUAR_XT002_BOMB_GRID_STEP          = 6.0f;
constexpr int   ULDUAR_XT002_BOMB_GRID_ROWS          = 4;
constexpr int   ULDUAR_XT002_BOMB_GRID_COLS          = 3;
```

Do **not** add the PR's `NPC_XT002_VOIDZONE = 34001` — `PB_NPC_XT002_VOID_ZONE` already exists.

Delete `GetXT002KillTarget` ([UldBossHelper.cpp:610](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L610)) and
its declaration. Its only caller is the mark action, which goes away, and the replacement priority
order is role-aware in a way a free function cannot express cleanly.

### 2. `src/Ai/Raid/Uld/Action/UldActions_XT002.h` / `.cpp`

**Remove:** `XT002MarkKillTargetAction` (RTI), `XT002BoombotRangedKillAction` and
`XT002AttackHeartAction` (both folded into the priority action), and the `RtiTargetValue.h` include.

**Add `XT002RaidPositionAction : MovementAction`** — `"xt002 raid position action"`:
- Main tank further than `ULDUAR_XT002_MAINTANK_SPOT_TOLERANCE` → `MoveTo` the MT spot.
- `IsRangedDps` further than `ULDUAR_XT002_RANGED_SPOT_TOLERANCE` → `MoveTo` the ranged spot.
- Everything else → `false`.
- `MovementPriority::MOVEMENT_COMBAT`. No teleport branch.

**Add `XT002SearingLightCarrierAction : MovementAction`** — `"xt002 searing light carrier action"`:
- `false` for tanks (the MT must not leave the boss).
- Otherwise `MoveTo(ULDUAR_XT002_SEARING_LIGHT_SPOT)` when further than 1yd. The spot clears
  `ULDUAR_XT002_DEBUFF_SPREAD_RADIUS` from both the ranged spot (25.5yd) and the MT spot (41.7yd).

**Rewrite `XT002GravityBombCarrierAction::Execute`** ([UldActions_XT002.cpp:98](src/Ai/Raid/Uld/Action/UldActions_XT002.cpp#L98)):
- `!IsXT002HeartbreakActive(botAI)` → unchanged, `MoveClearOf(allies, ULDUAR_XT002_DEBUFF_SPREAD_RADIUS)`.
  No Void Zones exist yet, so there is nothing to park and melee keep their uptime.
- Heartbreak up → park:
  - Origin = `..._ORIGIN_MELEE` for melee, `..._ORIGIN_RANGED` for ranged.
  - Walk a `ROWS × COLS` grid in `+x/+y` at `STEP`, as a file-scope `constexpr` offset array — not the
    PR's function-local `static std::vector` built by a lambda.
  - One `boss->GetCreatureListWithEntryInGrid(voidZones, PB_NPC_XT002_VOID_ZONE, 100.0f)`.
  - A cell is valid when no Void Zone is within `ULDUAR_XT002_VOID_ZONE_RADIUS` **and**
    `bot->IsWithinLOS(candX, candY, originZ)` — the LOS test is what keeps a walled-off cell from
    being chosen, since room geometry cannot be verified statically. Same check `MoveClearOf` already
    makes at [UldActions_XT002.cpp:58](src/Ai/Raid/Uld/Action/UldActions_XT002.cpp#L58).
  - No valid cell → fall back to `MoveClearOf(allies, ...)`, not the PR's "stand on the occupied
    origin anyway".
  - Already within 1yd of the chosen cell → `return true` and hold.

**Add `XT002SetDpsPriorityAction : AttackAction`** — `"xt002 set dps priority action"`. SWP pattern
above, gathering in one pass over `nearest npcs`.

| Role | Order |
|---|---|
| Ranged DPS | Life Spark → Scrapbot → Boombot → Pummeller → Heart → XT-002 |
| Melee DPS | Life Spark → Scrapbot → Pummeller → Heart → XT-002 |

`isAllowedPriorityTarget` rules:
- **Boombot** — ranged only, and only at `>= ULDUAR_XT002_BOOMBOT_AVOID_RADIUS`. Replaces
  `XT002BoombotRangedKillTrigger` verbatim.
- **Heart** — carries the gates from `XT002AttackHeartTrigger`
  ([UldTriggers_XT002.cpp:160-179](src/Ai/Raid/Uld/Trigger/UldTriggers_XT002.cpp#L160-L179)): barred
  while any Life Spark lives; allowed unconditionally under `IsXT002HardModeActive`; otherwise only
  above `ULDUAR_XT002_HEART_SAFE_HP_PCT`. Keep the existing comment about why the Heart outranks add
  DPS, and add one noting this gate reads the **config** flag while the parking gate reads the
  **aura** — deliberately different predicates.
- **XT-002 itself** — barred while `IsXT002Submerged(botAI)`.

Same-entry tie-break: nearest to the bot.

Unchanged: `XT002MoveAwayFromDebuffedAllyAction`, both spread actions, `XT002BoombotAvoidAction`,
`XT002VoidZoneAction`, `XT002PummellerTauntAction`, `XT002RedirectThreatAction`.

### 3. `src/Ai/Raid/Uld/Trigger/UldTriggers_XT002.h` / `.cpp`

**Remove:** `XT002MarkKillTargetTrigger`, `XT002AttackKillTargetTrigger`,
`XT002BoombotRangedKillTrigger`, `XT002AttackHeartTrigger`, the `IsSkullOnXT002Add` helper, and the
`RtiTargetValue.h` include.

**Add:**
- `XT002RaidPositionTrigger` — boss up; **false** when the bot carries either debuff or when
  `XT002BoombotAvoidTrigger` / `XT002VoidZoneTrigger` would fire, so anchoring never fights an
  emergency mover; then the MT / ranged-DPS distance checks.
- `XT002SearingLightCarrierTrigger` — boss up, `bot->HasAura(GetXT002SearingLightSpellId(bot))`, not a
  tank. Mirrors `XT002GravityBombCarrierTrigger` ([:74](src/Ai/Raid/Uld/Trigger/UldTriggers_XT002.cpp#L74)).
- `XT002SetDpsPriorityTrigger` — boss up, `!botAI->IsTank(bot)`.

Keep `HasDebuffedAllyInRange` and its comment — the spread triggers still use it.

### 4. Wiring — `UldActionContext.h`, `UldTriggerContext.h`, `UldStrategy.cpp`

Register three new actions and three new triggers; delete the six removed registrations. Nodes in
[UldStrategy.cpp:125-176](src/Ai/Raid/Uld/UldStrategy.cpp#L125-L176):

```cpp
"xt002 searing light spread trigger"   -> "xt002 searing light spread action"   ACTION_EMERGENCY
"xt002 gravity bomb spread trigger"    -> "xt002 gravity bomb spread action"    ACTION_EMERGENCY
"xt002 boombot avoid trigger"          -> "xt002 boombot avoid action"          ACTION_EMERGENCY
"xt002 void zone trigger"              -> "xt002 void zone action"              ACTION_EMERGENCY
// carriers outrank the neighbours' step-out: the raid cannot spread away from someone walking into it
"xt002 gravity bomb carrier trigger"   -> "xt002 gravity bomb carrier action"   ACTION_EMERGENCY + 1
"xt002 searing light carrier trigger"  -> "xt002 searing light carrier action"  ACTION_EMERGENCY + 1

"xt002 pummeller taunt trigger"        -> "xt002 pummeller taunt action"        ACTION_RAID + 4
"xt002 set dps priority trigger"       -> "xt002 set dps priority action"       ACTION_RAID + 3
"xt002 redirect threat trigger"        -> "xt002 redirect threat action"        ACTION_RAID + 1
"xt002 raid position trigger"          -> "xt002 raid position action"          ACTION_RAID
```

Rewrite the block comment: the current one explains the skull/Heart split, which no longer exists.
State instead that position is deliberately the lowest node and yields to add control, because the
engine breaks on the first successful action.

### 5. `src/Ai/Raid/Uld/UldMultipliers.cpp` / `.h`

In `XT002TargetGuardMultiplier::GetValue`
([UldMultipliers.cpp:88-129](src/Ai/Raid/Uld/UldMultipliers.cpp#L88-L129)):

- After the `GetXT002` gate, zero `dynamic_cast<DpsAssistAction*>(action)` for non-tanks, so
  `xt002 set dps priority action` owns `current target`.
- Zero generic `dynamic_cast<MovementAction*>(action)` **only** when `botAI->IsRangedDps(bot)`, the bot
  carries neither debuff, and the action name is not in the XT-002 movement allowlist
  (`raid position`, `searing light carrier`, `gravity bomb carrier`, both spread actions,
  `boombot avoid`, `void zone`). Healers and melee DPS keep every generic mover.
- Update the `retargets` set: drop `"attack rti target"`, `"xt002 mark kill target action"` and
  `"xt002 boombot ranged kill action"`; add `"xt002 set dps priority action"`. Keep
  `"xt002 pummeller taunt action"` and `"xt002 redirect threat action"`.
- Keep the `CastHealingSpellAction` escape hatch — it is what stops a bot parked in a Gravity Bomb from
  being frozen with no heals.
- Comment that `attack rti target` is intentionally **not** zeroed: bots stop setting marks, but a
  human's mark still wins.

Update the class header comment to say it also stands down the generic DPS targeting and, for ranged
DPS, the generic movers.

### 6. `docs/raids/ulduar.md`

Fold in the Established Facts table above. Three of the four are not XT-002-specific and are expensive
to re-derive: the Heartbreak-aura gating, the engine's break-on-first-success, and `IsRanged`
counting healers.

## Verification

The module cannot be compiled in this environment — static checks here, build and in-game by you.

**Static:**
1. `grep -rn "rti\|Rti\|RTI\|skull" src/Ai/Raid/Uld/Action/UldActions_XT002.* src/Ai/Raid/Uld/Trigger/UldTriggers_XT002.*`
   → nothing.
2. Every `creators[...]` string in `UldActionContext.h` / `UldTriggerContext.h` matches a
   `NextAction(...)` / `TriggerNode(...)` string in `UldStrategy.cpp` character for character — a
   mismatch is silent at runtime.
3. `grep -rn "GetXT002KillTarget\|XT002MarkKillTarget\|XT002AttackHeart\|XT002BoombotRangedKill" src/`
   → nothing left behind.
4. Every name in the multiplier's movement allowlist matches a real registered action name.
5. New `Position` externs declared once in the header, defined once in the `.cpp`.

**Build:** no new compiler warnings — several classes gain `AttackAction` / `MovementAction` virtuals.

**In game:**
1. Bot raid into Ulduar, pull XT-002. Main tank settles ~9.5yd behind the boss at `(895.8, -12.5)`;
   ranged DPS form a loose cluster around `(866, -12.5)`; healers position themselves; melee ride the
   boss. Confirm ranged DPS actually cast without walking in — that is what the moved coordinate buys.
2. Watch for oscillation: a ranged DPS that walks to the spot, walks back toward the boss, then
   returns means the movement suppression is mis-scoped.
3. Searing Light lands → carrier runs to `(862.7, 12.8)`, everyone else holds. The spread action should
   now rarely fire.
4. Gravity Bomb **before** the first Heart dies → carrier steps clear dynamically, does *not* run
   south. **After** Heartbreak → carrier runs to the south origin and each Void Zone lands on a fresh
   grid cell, packing them into a block. Confirm no carrier ever runs at a wall and stalls.
5. Adds up → melee never target a Boombot, ranged shoot Boombots only from >12yd, Life Sparks pull
   everyone off whatever they were on.
6. **No raid icon is ever set by a bot.** Then mark a skull manually mid-fight and confirm bots honour
   it and that it survives the encounter.
7. Normal mode: the exposed Heart stops taking damage at 15%, Heartbreak never procs. Hard mode: the
   Heart is burned and burst cooldowns fire in the Heart window.

## Follow-up

- Copy this plan to `docs/plans/xt002-anchored-positioning/xt002-anchored-positioning.PLAN.md` per the
  project's planning-directory convention.
- Nothing goes upstream for now. PR #2653 has two defects worth reporting if we ever push our XT-002
  work up — its priority order puts Boombot above Scrapbot (Scrapbots heal XT; Boombots only cost
  damage), and its melee attack the Heart with no HP gate, accidentally triggering Heartbreak in a
  normal-mode raid. Not worth a review round-trip on a flat file layout our tree diverged from.
