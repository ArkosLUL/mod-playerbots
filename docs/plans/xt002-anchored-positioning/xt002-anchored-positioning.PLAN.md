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

## Amendment: prepositioning dropped, anchors kept

The anchors themselves stay. What went is the pre-pull walk to them: `XT002RaidPositionTrigger` now
requires `xt002->IsInCombat()`, and the ranged-DPS generic-mover stand-down in
`XT002TargetGuardMultiplier` carries the same gate, so a ranged bot standing near XT before the pull
can still follow its master.

Two targeting bugs fixed alongside it. The Heart is hidden rather than despawned when its window
shuts (`UNIT_FLAG_NOT_SELECTABLE`, `boss_xt002.cpp` `ACTION_DISPOSE_HEART`), and `IsAllowedTarget`
only checked `IsAlive()` - so a bot that was on the Heart held it for the rest of the fight while
every spell it queued failed against an untargetable unit. `IsAllowedTarget` now rejects
untargetable units and requires the Exposed Heart aura, and the sticky rule in `ResolveTarget` no
longer holds a target the gates just rejected.

## Amendment: carrier hold, no neighbour dodge, tanks off the adds

Three defects found in-game after the anchors landed. All three causes were traced in the source
rather than guessed at.

**Carriers walked back mid-debuff.** Both carrier actions return `false` once they are at their
destination, so the tick kept going: `xt002 set dps priority action` retargeted XT and `reach melee`
(`ACTION_HIGH + 1`) walked the carrier back with the debuff still on it. Nothing held them. Fixed in
`XT002TargetGuardMultiplier` rather than in the actions - a carrier action returning `true` would
starve every node below `ACTION_EMERGENCY + 1` for the full 9-10 s, and a debuffed healer would stop
healing. The movement sweep now covers anyone carrying either debuff, of any role, so generic movers
go to zero while the encounter's own movers and every cast keep running. `AttackAction` stays exempt:
`AttackAction::Attack` never moves the bot, and zeroing it kills the encounter's own targeting.

**The Gravity Bomb carrier did not go far enough before Heartbreak.** It cleared only
`ULDUAR_XT002_DEBUFF_SPREAD_RADIUS` (12 yd) from the nearest ally. New
`ULDUAR_XT002_DEBUFF_CLEAR_RADIUS = 25.0f` drives the dynamic step-out instead. The fixed south origin
was rejected for this case: no Void Zone exists before Heartbreak, and ~6 s of travel each way against
a ~9 s debuff would cost a melee carrier most of a 20 s cycle. Post-Heartbreak grid parking is
unchanged, and the Searing Light spot did not move - it already clears the splash from every anchor
with double the margin.

**The raid dodged its own carriers.** `xt002 searing light spread` / `xt002 gravity bomb spread` fired
at `ACTION_EMERGENCY` and moved every neighbour, costing raid DPS for a mechanic the carrier now
solves alone. Both triggers, both actions, their `XT002MoveAwayFromDebuffedAllyAction` base and the
`HasDebuffedAllyInRange` helper are gone. `XT002GravityBombCarrierTrigger` also dropped its
`GetNearestPlayerInRadius` condition: with nobody else moving, the carrier leaves regardless.

**Tanks dragged XT into the add pile.** `tank target` resolves through
`FindTankTargetSmartStrategy::IsBetter`, which has two routes into the same failure.
`GetIntervalLevel` returns 2 for any unit the tank lacks aggro on against 1 for one it is already
tanking in melee, so every Scrapbot, Boombot and Pummeller outranks XT by construction. With a raid
`MEMBER_FLAG_MAINTANK` set and 2+ alive tanks the sticky branch takes over instead, and that is worse:
a submerged XT carries `UNIT_FLAG_NOT_SELECTABLE` and drops out of `attackers` entirely, so during a
Heart phase the tank latches onto an add and the sticky rule holds it there after XT resurfaces.
Scrapbots enter `attackers` as soon as any bot damages one, through `GetThreatenedByMeList`.

`XT002SetDpsPriorityTrigger` now fires for tanks too, and the priority list gives the Pummeller tank
`[Pummeller, XT]` and every other tank `[XT]`. `ResolveTarget` skips its `dps target` fallback for
tanks - that fallback resolves to an add and would reintroduce the chase. `TankAssistAction` is zeroed
for tanks while XT is in combat, which takes both routes out at once. The off-tank no longer picks up
Scrapbots, which is intended: Scrapbots are a DPS race, Boombots must never be tanked, and the
Pummeller is already owned by `xt002 pummeller taunt action`.

Tanks are deliberately kept off the Heart. The normal-mode floor would cover them, but tank damage on
the Heart is a small gain against a real risk of flipping the raid into hard mode. Through the ~30 s
Heart window the main tank holds his spot with nothing to hit.

`IsXT002PummellerTank` in `UldBossHelper` now carries the assist-tank-then-main-tank selection that
`XT002PummellerTauntTrigger` had inlined, so the taunt and the tank priority list cannot drift apart.

## Amendment: main tank unpinned, Pummeller off the boss tank, hard mode reachable

Four defects observed in game after the amendment above shipped. All four causes were traced in
source, the core script and `acore_world`; none is inferred from behaviour alone.

### Defect 1 — the main tank cannot take XT back after losing aggro

`XT002RaidPositionTrigger::IsActive` fired for the main tank on distance alone, and
`xt002 raid position action` sits at `ACTION_RAID` (60) against `reach melee` at 21. The engine breaks
at the first action returning true, so while the tank was >3 yd off the anchor the anchor won every
tick and `reach melee` never ran — the tank was pinned to a fixed point.

Harmless while he holds the boss, since XT follows his victim and comes with him. Fatal when he does
not: XT walks to whoever took aggro, the tank walks to `(895.82, -12.54)` and stands there out of melee
range generating no threat, forever.

He cannot taunt out of it either. XT-002 is `type = 9` (`CREATURE_TYPE_MECHANICAL`) with `type_flags`
32876 — no `CREATURE_TYPE_FLAG_BOSS_MOB` bit, so `isWorldBoss()` is false — and he is a vehicle, since
the Heart and every Scrapbot ride him. That hits the mechanical-non-worldboss block of
`Vehicle::ApplyAllImmunities` (`src/server/game/Entities/Vehicle/Vehicle.cpp:160-186`), which applies
`SPELL_AURA_MOD_TAUNT` and `SPELL_EFFECT_ATTACK_ME` immunity. Taunt, Growl, Dark Command and Hand of
Reckoning are all dead on him, so threat from damage is the only route back and the anchor was what
denied it.

Ignis already carried the guard XT was missing (`UldTriggers_Ignis.cpp:55-59`): the main-tank anchor
stands down unless `boss->GetVictim() == bot`.

XT spawns at `(886.28, -12.05)` and the anchor is 9.5 yd behind him, so the anchor is only in melee
range while XT is at home — a pull by a DPS bot or by the player reproduced this from second one.

### Defect 2 — the boss tank dragged XT into the Pummeller

`BuildPriorityList` handed `[Pummeller, XT]` to whoever `IsXT002PummellerTank` selected: the first
assist tank, falling back to the main tank when `GetGroupAssistTank(botAI, bot, 0)` returns null. That
fallback fires with one alive tank, a dead off-tank, or a second tank whose spec `IsTank` does not read
as one. The tank holding XT then targeted the add, `reach melee` walked him at it with the boss in tow,
and the anchor yanked him back — the observed bounce.

The toy piles are ~80 yd from the tank anchor (`(897.9, 67.1)`, `(898.1, -88.9)`, `(793.1, -95.2)`,
`(792.6, 65.4)`), so this dragged the boss the length of the room.

### Defect 3 — hard mode was unreachable, because the Heart ranked below the adds

`IsAllowedTarget` opens the Heart unconditionally in hard mode, but in the non-tank priority list the
Heart sat below Life Sparks, Scrapbots, Boombots and Pummellers. The Heart casts `SPELL_HEART_OVERLOAD`
on expose and its Exposed Heart aura fires an Energy Orb at a toy pile on every hit it takes, on a
1.5 s cooldown (`boss_xt002.cpp:957-994`) — adds keep spawning *because* the Heart is up and being hit,
so a raid that clears adds first never reaches it inside the 30 s window. Both tanks were idle for that
whole window as well: the Heart was not on the tank list and XT is submerged.

### Defect 4 — the hard-mode burst window was dead code

`XT002BurstWindowMultiplier` opens burst on the exposed Heart in hard mode, but the shared
`HoldBurstUntilTankEngagedMultiplier` runs against the same actions and multipliers multiply, so a 0
from either closes the gate. The Heart clears that multiplier's boss check (`type_flags` 524396 carries
`CREATURE_TYPE_FLAG_BOSS_MOB`) but then falls into `TankHasHeldBoss`, which reads `boss->GetVictim()`.
The Heart is a `NullCreatureAI` riding XT with no threat table, so that is always null and burst was
always 0 — bots held every cooldown through the Heart window and only spent after Heartbreak.

Ruled out along the way: Heart damage is transferred by `SetData(DATA_TRANSFERED_HEALTH, …)` rather
than a damage event, so hitting the Heart builds no threat on XT and Heart phases are not what flips
the main tank's aggro.

### Decisions

1. **The tank anchor yields whenever XT has a victim that is not this bot**, matching Ignis. "No victim"
   deliberately keeps the anchor — that is the Heart window, where standing on the spot is right.
2. **No boss-taunt node.** XT is taunt-immune at the core level. Recovery is threat from damage, which
   is exactly what unpinning restores.
3. **The tank holding XT taunts the Pummeller but never targets it.** Taunt reaches 30 yd, so the add
   walks to the tank rather than the tank walking 80 yd to the add. Taunt ownership and target
   ownership stop being the same thing, and `IsXT002PummellerTank` keeps its meaning as the taunt owner.
4. **The Pummeller gate is "is there a second alive tank", not "am I the main tank".** A dead flagged
   main tank leaves `IsMainTank` false for the one surviving tank, who would then chase with the boss in
   tow; `GetGroupTankNum(bot) > 1` covers that and the single-tank case in one condition.
5. **The Heart goes to the top of the non-tank list in hard mode only**, and **tanks join the burn in
   hard mode only**. Normal mode is untouched: a tank hit there is exactly what would flip the raid into
   hard mode by accident, and the `ULDUAR_XT002_HEART_SAFE_HP_PCT` floor still applies.
6. **The burst-gate fix goes in the shared multiplier, scoped to vehicle passengers.** A boss-flagged
   passenger has no threat table, so waiting for a tank to hold it waits forever. The encounter
   multiplier cannot override it — a 0 anywhere wins — so the fix has to live in the generic gate.

## Amendment: Void Zone parking inside the debuff, and no chasing adds that never left their pile

Three more defects, reported from hard-mode pulls and traced against module source, `boss_xt002.cpp`,
`acore_world`, the DBC reference and a `navprobe` sweep.

### Defect 1 — the Gravity Bomb carrier drops its Void Zone in the raid

Gravity Bomb lasts **9 s** (63024 and 64234, `DurationIndex` 105), and past Heartbreak the Void Zone
spawns where the debuff expires. `ParkVoidZone` walked the 4x3 grid in index order and took the first
free cell — and cell 0 is the origin, the corner **furthest from the raid**. Melee ran 44.5 yd from XT
and ranged 49.8 yd from their anchor, 6.4 s and 7.1 s of a 9 s window at 7 yd/s, before react delay or
any path detour. That was the worst case on every single bomb.

Separately, it returned `MoveTo`'s value, and `MoveTo` goes false while the bot is already walking to
that exact cell — `IsDuplicateMove` holds for 5 s (`maxWaitForMove`) once the destination matches within
0.01 yd. `Execute` read that false as "no cell available" and fell through to `MoveClearOf(allies, 25)`,
a local ring search that knows nothing about Void Zones or the grid.

That fall-through was masked by coincidence: `IsWaitingForLastMove` blocked the competing move, and its
window is `MoveDelay = distance / speed` capped at 5 s, which for a 44.5 yd run expired at the same
instant the duplicate window did. **Any travel under 5 s opens the gap** — the duplicate test still
bites, the wait test does not, and `MoveClearOf` issues a fresh destination that turns the carrier
around mid-run. Shortening the run without fixing the return value would have made this worse.

### Defect 2 — bots walked 100 yd at adds that never left their toy pile

`npc_xt_toy_pile::SpellHit` only summons from a pile **more than 90 yd** from XT. The four piles sit at
80.0, 77.8, 124.9 and 121.6 yd from XT's spawn, so the two east piles never fire and every add spawns at
a west pile 122-125 yd out. With `sightDistance` at 100, a bot first sees one at roughly 100 yd — and an
add that fails to path away from its pile stays visible, alive and stationary for the rest of the fight.

`BuildPriorityList` handed that add to every non-tank, healers included. Healers were the visible case
because nothing stopped them: `HealerDPSMapRestriction = 1` with map 603 in `RestrictedHealerDPSMaps`
means a healer on Ulduar has no `healer dps` strategy and so no damage node that could use a target, but
`GenericPriestStrategy : RangedCombatStrategy : CombatStrategy` still gives it `enemy out of spell` ->
`reach spell` at `ACTION_HIGH`, and `XT002TargetGuardMultiplier` suppresses generic movers only for
carriers and ranged DPS. The one thing a healer's target did here was walk it at the furthest add in the
room. Melee took the same chase through `reach melee`; ranged DPS, movers zeroed, sat holding an add
they could not reach and stopped hitting the boss.

Two amplifiers found while tracing it. `ResolveTarget` fell back to `"dps target"` — the generic
nearest-attackable picker with no leash — so any gate on the priority list was bypassable the moment
every listed candidate failed. And all three Pummeller sites used `GetFirstAliveUnitByEntry`, which is
first-found rather than nearest, so a stuck Pummeller masked the one hitting the raid.

### Defect 3 — the parking grid could run out

Void Zones live **180 s** (64203, `DurationIndex` 25) against a bomb every **16 s** in 25-man, so ~11
are alive at once across two grids of 12 cells. A full grid made `ParkVoidZone` return false, which is
the defect-1 fall-through at the worst possible moment.

### Ruled out

Bots do not dodge Life Sparks and cannot: Static Charged (64227) is `APPLY_AREA_AURA_ENEMY` +
`PERIODIC_DAMAGE` with `EffectRadiusIndex` 30 — **500 yd** — for 800 nature every 3 s, permanent while
the spark lives, so position changes nothing. No avoid node for entry 34004 exists, and `avoid aoe`
cannot match one: it is not a `DYNOBJ_AURA_TYPE`, `AvoidUnitWithDamageAura` bails at
`!HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE)` and looks for `PERIODIC_TRIGGER_SPELL`, and 500 exceeds
`MaxAoeAvoidRadius` (15.0) regardless. What looked like dodging was melee making the 34 yd round trip to
the Life Spark spot.

### Decisions

**Parking**

1. **Nearest free cell, not first in index order.** Melee 30.2 yd, ranged 28.9 yd — ~4.2 s of the 9 s.
2. **Prefer a cell whose straight-line approach is clear of live Void Zones**, margin 7.5 yd, falling
   back to the nearest free cell when none is clean. There is no puddle dodging while carrying: the
   carrier action sits at `ACTION_EMERGENCY + 1` and starves `xt002 void zone action` at
   `ACTION_EMERGENCY`, so the segment test is the only protection on the way in. It is an
   approximation — `MoveTo` follows a navmesh path, not the line measured.
3. **`ParkVoidZone` returns a tri-state and never `MoveTo`'s value.** "Already walking there" is not
   "nowhere to go". Mirrors Sapphiron's `ShelterResult`: latch on arrival, `StopMoving()`, arrival
   deadband 2.0, re-engage 5.0 — tighter than Sapphiron's 3.5/6.0 because the puddle lands at the
   carrier's feet and the destination is static.
4. **The parked carrier yields the tick**, matching `XT002SearingLightCarrierAction`, which already
   returns false on arrival. Otherwise it stands mute for ~4.5 s of every 16 s; healing actions are not
   `MovementAction`s, so a parked healer carrier can heal, and a hunter can shoot XT from the melee lot.
5. **The grid grew to 5x4 and grows away from the raid.** The origin moved to the raid-facing edge and
   the walk runs +x / -y, so the near edge is unchanged and every added cell is further out. Both new
   origins are cells of the old grid; all 40 cells were navprobe-verified on mesh (worst distance to
   poly 0.51 yd, worst settled Z 409.255 against an origin Z of 409.804).
6. **Occupancy margin stays at 6.0.** Accepted consequence: a carrier settling 2.0 off centre can be
   4.0 from a live puddle, inside `XT002VoidZoneTrigger`'s radius, and get nudged off its cell once it
   yields. The nudge is away from the puddle, inside a lot the raid never enters, and standing in
   Consumption while carrying is worth moving for. Widening the margin would halve capacity; widening
   the step would void the navprobe sweep.

**Targeting**

7. **Healers get no target, and their stale one is cleared.** Declining to set one is not enough — a
   leftover from the previous wave fires `reach spell` just as well. Cleared narrowly, **without**
   `ChangeEngine`: the generic `drop target` also switches the bot to the non-combat engine and would
   stop it healing.
8. **Tanks are never cleared.** A tank's stale target is usually an add it is holding.
9. **The `"dps target"` fallback is dropped for every role.** It is the unleashed nearest picker, so it
   handed straight back the add the leash had just rejected.
10. **Pile adds are leashed to 60 yd from XT.** Clears all four pile positions (77.8 yd nearest) with
    margin, and stays correct if the upstream 90 yd condition is ever flipped. Life Sparks are exempt —
    they spawn on the Searing Light carrier and chase players, so a distance-from-XT gate is the wrong
    shape for them.
11. **Role reach gates on top, measured from the bot**: ranged 35 yd, melee 15 yd, applied to the three
    pile adds and to Life Sparks, never to XT or the Heart, and **never to tanks** — `IsAllowedTarget`
    runs on the tank list too, and a 15 yd gate would break the off-tank's Pummeller chase. 35 clears
    the Life Spark spot (25.5 yd from the ranged anchor) by ~10 yd; 15 costs melee nothing, because
    Scrapbots `MoveFollow(xt002)` and arrive at the boss, Boombots are already excluded from melee
    lists, Pummellers are tank-only, and a Life Spark chases the Searing Light carrier home in seconds.
12. **The taunt gates on taunt range (30 yd), not the leash** — Taunt, Growl, Dark Command and Hand of
    Reckoning are all `RangeIndex` 4. A node at `ACTION_RAID + 4` should not fire against an add it
    cannot reach.
13. **One helper, XT leash always applied, bot reach layered on top.** The leash is unconditional for
    every add, so making it a parameter would invite a call site that forgets it.

**Adjacent**

14. **`ULDUAR_XT002_DEBUFF_SPREAD_RADIUS` deleted.** Nothing read it, and its comment asserted a 12 yd
    spread that contradicts the design — the raid holds still and the carrier solves its own mechanic.
    The distance still explains the spot placements, so it lives in those comments as prose.
15. **`ULDUAR_XT002_SEARING_LIGHT_SPOT` did not move**, only its comment. The spark chases the carrier
    home either way, and moving the spot would drag the Searing Light splash toward the raid.
16. **The upstream toy-pile condition is noted, not patched.** `xt002->IsWithinDist(me, 90.0f)` reads
    inverted against its own comment, but it is upstream AzerothCore (`c09afb080`, PR #26345) and
    changing it would alter encounter difficulty. The leash is robust either way: flipped, the piles
    that summon become the east pair at 77.8 yd, still outside 60.

## Amendment: one mover per bot, carriers and hazards merged

Reported after the previous amendment shipped: bots still drop the Gravity Bomb Void Zone at their
feet. The observed case was a ret paladin holding **Searing Light and Gravity Bomb at the same time**,
who dropped the puddle in the middle of the raid.

### The defect

Two encounter nodes both moved the same bot, and the engine has no mechanism that stops them.

`xt002 gravity bomb carrier action` and `xt002 searing light carrier action` were both registered at
`ACTION_EMERGENCY + 1` — a tie. `Queue::findHighestRelevanceBasket` breaks ties with a strict `>`, so
the earliest basket in the list wins, and `Queue::Push` only ever appends. A basket that loses a tick
is not removed, so it sits ahead of the winner when the winner is re-pushed next tick, and the two
swap ownership of the tick.

Raising one node's relevance would not have fixed it. Relevance orders the queue, it does not grant
exclusivity: the loser pops the moment the winner returns false, and the Gravity Bomb carrier returns
false by design once parked, so that a healer carrier can heal and a hunter can shoot. Decision 4 of
the previous amendment is what the Searing Light carrier was claiming:

| t | what happened |
|---|---|
| 0.0 s | bomb lands, bot in the melee line. Gravity Bomb carrier wins, walks ~30 yd to a lot cell |
| ~4.2 s | parks, `StopMoving()`, returns false so it can keep casting |
| 4.2 s | Searing Light carrier claims the freed tick, walks the bot 55 yd back north |
| 9.0 s | debuff expires ~33 yd along that line |

The melee lot to the Searing Light spot is 55.5 yd and the line passes **0.8 yd from the ranged anchor
at 4.3 s in**, so the puddle lands on the ranged stack. From the ranged lot it is 59.6 yd, passing
13.8 yd from the anchor at 5.5 s.

The same tie existed between `xt002 boombot avoid action` and `xt002 void zone action`: both at
`ACTION_EMERGENCY`, both `MoveAwayFromCreatureAction`, both returning `MoveTo`'s value, both pulling
in opposite directions. A melee near a Boombot and a puddle alternated destinations every tick and
slid in place inside both.

### Established facts

| Fact | Source |
|---|---|
| Searing Light 63018 / 65121 and Gravity Bomb 63024 / 64234 all have `DurationIndex` 105 → 9000 ms | `spell.reference.csv`, `spellduration.reference.csv` |
| Both fire every 16 s in 25-man (20 s in 10-man), rescheduled to 25 s / 33 s past Heartbreak, and Tympanic Tantrum delays the whole `GROUP_SEARING_GRAVITY` by 10 s, so the nominal offset drifts and the two overlap | `boss_xt002.cpp:199-200`, `:275-276`, `:367-374` |
| Target selection removes only XT's current victim and then picks at random, so nothing prevents one player holding both — and the main tank can hold neither | `boss_xt002.cpp:719-725` |
| The Void Zone and the Life Spark are both summoned **on the player**, where the debuff expires | `boss_xt002.cpp:743-748`, `:767-773` |
| `Queue::Push` appends and never reorders; `findHighestRelevanceBasket` uses a strict `>`; a basket is removed only by `Pop` or `RemoveExpired` | `Queue.cpp:11-27`, `:76-101` |
| A trigger going false does not dequeue its basket, and `AiPlayerbot.ExpireActionTime` is 5000 ms, so a stale basket can still fire up to 5 s later — longer than half the debuff | `Engine.cpp:259`, `:447-487`, `PlayerbotAIConfig.cpp:97` |
| `MoveAwayFromCreatureAction` **maximises** distance to the nearest hazard over a 30 yd ring, so stepping off a puddle is a relocation of up to 30 yd rather than a nudge | `MovementActions.cpp:2865-2905` |
| Melee lot origin to Searing Light spot 55.5 yd, within 0.8 yd of the ranged anchor at t=0.54; ranged lot origin 59.6 yd, within 13.8 yd at t=0.64; lot cells sit 29-56 yd from the ranged anchor | computed from the shipped constants |

### Decisions

1. **The two carrier nodes are merged into one.** `xt002 debuff carrier` fires on either debuff and
   resolves a single destination per tick internally. One mover means the tie cannot exist, in this
   state or any future one. Guarding the Searing Light node instead would have left two nodes sharing
   a relevance and a queue.
2. **The two hazard nodes are merged the same way** into `xt002 avoid hazard`, which clears Boombots
   and Void Zones in one move.
3. **Gravity Bomb outranks Searing Light on a double carrier.** The puddle denies raid floor for
   180 s; the 12 yd splash lasts 9 s and expires over an empty parking lot, so it hits nobody. The
   Life Spark then spawns in the lot, 29-56 yd from the ranged anchor, and walks in by itself. The
   reverse would put a 180 s puddle on the one spot reserved for sparks.
4. **The carrier keeps ownership while the bot holds either debuff.** When the bomb expires with
   Searing Light still ticking the bot is standing in its own fresh puddle, so holding position is not
   an option. That puddle makes its old cell test occupied, so the picker selects the nearest other
   free cell - a 6-12 yd hop to a navprobe-verified spot, still inside the lot.
5. **The hazard node ignores Void Zones while the bot carries a debuff** and always honours Boombots.
   A carrier's cell selection already owns where it stands relative to puddles, and letting the
   generic dodge fire would fling it up to 30 yd - from inside the lot, right after its own bomb
   expired, in whatever direction happened to be emptiest.
6. **Cell selection prefers cells clear by `VOID_ZONE_RADIUS + BOMB_CELL_ARRIVED` (8.0) and falls back
   to `VOID_ZONE_RADIUS` (6.0).** A preference, not a gate: a bot parked at the edge of the 2.0
   deadband would otherwise sit 4.0 from a puddle, inside Consumption. As a hard gate one puddle would
   block five cells of twenty and exhaust the lot, so it degrades instead of failing. This supersedes
   decision 6 of the previous amendment, which accepted the nudge on the assumption it was small.
7. **Hazard moves take the nearest sufficient point, not the furthest.** The merged node uses the
   `MoveClearOf` ring search, which stops at the first spot clearing everything, rather than
   `MoveAwayFromCreatureAction`'s maximise-distance sweep.
8. **Tank rules are unchanged.** Tanks do not reposition for Searing Light, because dragging XT or
   abandoning a Pummeller costs more than the splash; tanks do park for Gravity Bomb, and a Pummeller
   following an off-tank into the lot is fine. The main tank can hold neither debuff anyway.
9. **`MoveClearOf` takes per-unit clearances**, `std::vector<std::pair<Unit*, float>>`, so one ring
   search serves both the pre-Heartbreak ally spread at 25 yd each and the mixed hazard list of
   Boombots at 12 and Void Zones at 6.

Left alone deliberately: a carrier walking to its cell still ignores Boombots, because the carrier
outranks the hazard node. The run leads away from XT where the Boombots mostly are, and adding Boombot
terms to the approach test is a separate change.

## Amendment: a reachable parking lot, stopping short when it is not, and an anchor for healers

Two reports off the same branch: carriers still drop Void Zones in the raid, and healers trail the
master around the room. The node merge above did close the tug-of-war it was aimed at — this is a
different cause, and it is arithmetic rather than an engine race.

### The lot is further away than the debuff is long

`ParkVoidZone` picks a cell and `Execute` returns true for as long as the bot walks towards it.
Nothing tracks how long the debuff has left or how fast the bot is actually moving, so a carrier that
cannot make it in time keeps walking and drops the puddle wherever it stands at t=9s — which, early
in the run, is still the raid.

| | distance | at 7.0 y/s | at 3.5 y/s |
|---|---|---|---|
| melee lot cells, from XT's spawn | 30.1 - 50.2 yd | 4.3 - 7.1 s | 8.6 - 14.3 s |
| ranged lot cells, from the ranged anchor | 28.9 - 54.8 yd | 4.1 - 7.8 s | 8.3 - 15.7 s |

Gravity Bomb runs 9.0 s. Two things spend that budget:

- **Tympanic Tantrum halves the raid's movement speed.** 62776 is an 8000 ms channel ticking every
  1000 ms; each tick casts 62775 at every enemy inside radius index 30 — 500 yd, the whole room —
  and its second effect is `SPELL_AURA_MOD_DECREASE_SPEED` at -51 base for duration index 35, 4000 ms.
  Re-applied every second, so the raid runs at half speed for the channel plus roughly four seconds
  after it. Nothing in either lot is reachable under it. The boss does push the debuff group back 10 s
  when the tantrum starts, but a bomb applied up to 9 s *before* it still overlaps.
- **The cell picker preferred room over distance.** The rank was `room * 2 + approachClear`, with
  distance only breaking ties, so a far cell with 8 yd of clearance beat a near one with 6 — spending
  the budget on cells 7-8 s away even at full speed.

The fallback made it worse rather than better: `ParkResult::None` fell through to `MoveClearOf(allies)`,
a ring search anchored on wherever the bot happens to be. From the middle of the raid that is a short
sideways shuffle, and it reads no Void Zones at all.

### Healers were never anchored

`XT002RaidPositionAction` returns false for them — only the main tank and ranged DPS had spots — and
`XT002TargetGuardMultiplier` suppresses generic movers only for carriers and ranged DPS, so healers
kept every one. What positioned them was `reach party member to heal` (relevance 38-40 by spec) and
`combat formation move` disperse, both following whoever is taking damage. `follow` is non-combat
only, so it was not the mechanism, but the effect was the same.

### Established facts

| Fact | Source |
|---|---|
| Gravity Bomb 63024/64234: EFFECT_0 periodic trigger, period 9000 ms, so one burst at expiry, into 63025/64233 = SCHOOL_DAMAGE at radius index 32 (12 yd) plus `SPELL_EFFECT_PULL_TOWARDS` at radius index 9 (20 yd), both `TARGET_UNIT_DEST_AREA_ALLY` | `spell.reference.csv`, `spellradius.reference.csv` |
| Searing Light 63018/65121: EFFECT_0 periodic trigger, period 1000 ms, into 63023/65120 = SCHOOL_DAMAGE at radius index 14 (8 yd), `TARGET_UNIT_DEST_AREA_ALLY`, 2249 base in 10-man. It damages bystanders every second for the full 9 s | same |
| Tympanic Tantrum 62776: duration index 31 = 8000 ms, period 1000 ms, into 62775 at radius index 30 (500 yd), `TARGET_UNIT_SRC_AREA_ENEMY`, effect 2 = `SPELL_AURA_MOD_DECREASE_SPEED` base -51, duration index 35 = 4000 ms | same |
| The tantrum calls `events.DelayEvents(10s, GROUP_SEARING_GRAVITY)` before casting | boss_xt002.cpp, `EVENT_TYMPANIC_TANTRUM` |
| Consumption's damage half is 64208 at radius index 8 = 5 yd, with no growth mechanic, so `ULDUAR_XT002_VOID_ZONE_RADIUS` = 6.0 is a correct 1 yd buffer | `spell.reference.csv`, `npc_xt_void_zone` |
| No adds exist after Heartbreak: toy piles summon only when hit by the Heart's energy orb, and `RescheduleEvents` omits `EVENT_PHASE_CHECK` once `_hardMode`, so there is no further Heart phase | boss_xt002.cpp |
| XT-002 (33293) spawns at (886.275, -12.0545, 409.602) on map 603 | `acore_world.creature` |
| `bot->GetSpeed(MOVE_RUN)` already carries the tantrum slow, and `MoveDelay` is `distance / GetSpeed(MOVE_RUN)` | `MovementActions.cpp` |
| `avoid aoe` is a default action at `ACTION_EMERGENCY`, the same relevance as `xt002 avoid hazard action`, pushed every tick by `PushDefaultActions` - but only added when `autoAvoidAoe && HasGameClientMaster()` | `CombatStrategy.cpp`, `AiFactory.cpp`, `Engine.cpp` |
| The Void Zone (34001) carries `unit_flags` 33554432 = `UNIT_FLAG_NOT_SELECTABLE`, so `AvoidAoeAction::AvoidUnitWithDamageAura` does see it | `acore_world.creature_template` |
| `FleePosition` caps travel at `AiPlayerbot.FleeDistance`, default 5.0, and reads one hazard, so `avoid aoe` is a nudge and not a relocation | `docs/engine/pitfalls.md`, `MovementActions.cpp:2152,:2215` |
| The shipped ranged anchor (866.0, -12.5, 409.8) settles to `UpdateAllowedPositionZ` 409.803 on map 603 | navprobe |

### Decisions

1. **The cell picker is time-budgeted.** Reachability inside the aura's remaining duration, at the
   bot's current speed, ranks above room and room still ranks above a clear approach.
   `GetSpeed(MOVE_RUN)` carries the tantrum slow, so nothing in the encounter code has to know the
   tantrum exists.
2. **When nothing is reachable the carrier stops short.** It keeps the bearing to the best cell and
   walks as far along it as the budget allows, taking that point only when it is at least
   `ULDUAR_XT002_GRAVITY_BOMB_PULL_RADIUS` (20 yd, the real pull radius) from every living raid
   member. The puddle lands on the lot approach instead of in the raid. If the point fails that test
   the ring search gets the tick, because it maximises clearance rather than following a fixed
   bearing.
3. **The stop-short point needs no latch.** As the bot advances by `speed * dt` the remaining
   duration drops by `dt`, so reach and distance to the cell shrink together and the point holds
   still. A slow landing mid-run correctly drags it back towards the bot; clamped at zero so the bot
   never walks backwards.
4. **Only a bot actually holding the bomb is budgeted.** The stepping-off-own-bomb case is a 6-12 yd
   hop with no deadline on it, so it passes an unlimited reach.
5. **Healers share the ranged anchor with a 10 yd band.** No new coordinate: the shipped anchor is
   29.8 yd from the tank spot and about 20 yd from the melee stack, both inside 40 yd of heal range,
   and 30.0 yd from the nearest melee parking cell, clear of the 20 yd pull.
6. **Healers keep their generic movers.** They are not added to the multiplier's suppressed set. The
   anchor sits at `ACTION_RAID` and outranks both `reach party member to heal` and
   `combat formation move`, so it reels them in when they drift while heal range and disperse still
   decide where they stand inside the band - which is what stops six healers stacking on one point.
7. **The healer anchor stands down while a heal target is out of spell range.** It outranks
   `reach party member to heal`, so without that a healer could never close on a carrier parked out
   in the lot: the far cells are 55.6 yd from the anchor against 40 yd of heal range.
8. **`avoid aoe` is zeroed for everyone during XT's combat.** `xt002 avoid hazard action` answers
   both hazards this fight has and is meant to be the only thing moving a bot for them. Nothing is
   lost - Tympanic Tantrum is room-wide so it exceeds `maxAoeAvoidRadius`, and the Life Spark has no
   damage aura.
9. **Searing Light stays carrier-only.** It does damage bystanders - 8 yd, every second, for 9 s -
   and nothing steps out of it: no XT node moves a non-carrier away from a carrier, and generic
   `avoid aoe` cannot see it either, since its dynobj branch needs `DYNOBJ_AURA_TYPE` and its unit
   branch needs a `NOT_SELECTABLE` trigger NPC, and a player is neither. The carrier needs 4-6 s to
   clear (41.7 yd from the tank spot, 25.5 yd from the ranged anchor), so bystanders eat most of the
   ticks, and a Pummeller off-tank irradiates the melee stack for the whole debuff because tanks
   never move for it. Accepted: 25 bots scattering costs more than the damage does.

Two things left alone deliberately. Both parking lots sit on Boombot lanes from the toy piles at
(898.1, -88.9) and (793.1, -95.2), and `XT002AvoidHazardTrigger` checks Boombots before the carrying
gate - but the lot only exists after Heartbreak and there are no adds after Heartbreak, so the two
halves of the hazard node are never live at the same time. And two carriers can pick the same cell,
since there is no reservation: bombs are 16 s apart against a 9 s duration so two bomb carriers never
overlap, and the only bot that can collide is one sitting out a Searing Light, which drops nothing.

### Addendum: bubbles drop the puddle early

A second way a Void Zone lands in the raid, found while checking whether Hand of Freedom could help a
carrier outrun the tantrum. It cannot - 62775 carries `Mechanic = 0` and no effect mechanic, while
Hand of Freedom is two `SPELL_AURA_MECHANIC_IMMUNITY` effects keyed to `MECHANIC_ROOT` (7) and
`MECHANIC_SNARE` (11), so it neither strips the slow nor blocks the next tick. Same for the PvP
trinket, Every Man for Himself, and shapeshift's `RemoveAurasWithMechanic(ROOT|SNARE)`. Flat speed
buffs do work, since `UpdateSpeed` applies the largest positive modifier and then multiplies by
`(100 + slow) / 100` - Sprint under the tantrum gives 5.25 y/s against 3.5 - but only some classes
carry one and the carrier is whoever the boss picked.

What does matter:

| Fact | Source |
|---|---|
| Gravity Bomb 63024/64234 is `SchoolMask = 32` (shadow), is not passive, and lacks `SPELL_ATTR0_NO_IMMUNITIES`, so `SpellInfo::CanDispelAura` permits removing it | `spell.reference.csv`, `SpellInfo.cpp` |
| Divine Shield 642 and Ice Block 45438 both apply `SPELL_AURA_SCHOOL_IMMUNITY` with misc **126** (every magic school) and both carry `SPELL_ATTR1_IMMUNITY_PURGES_EFFECT` | `spell.reference.csv` |
| `AuraEffect::HandleAuraModSchoolImmunity` walks the target's applied auras on apply and removes every harmful one whose school intersects the immunity mask | `SpellAuraEffects.cpp` |
| `spell_xt002_gravity_bomb_aura::OnRemove` is an `AfterEffectRemove` hook with no removal-mode check, so it summons the Void Zone however the aura went away | boss_xt002.cpp |
| Bots cast both from "critical health" at relevance 90 | `GenericPaladinStrategy.cpp:25`, `GenericMageStrategy.cpp:147` |
| Hand of Protection 1022 is misc 1, physical only, so it strips neither debuff. Anti-Magic Shell uses `SPELL_AURA_MOD_IMMUNE_AURA_APPLY_SCHOOL` with no purge attribute, so it blocks the bomb landing rather than dropping one | `spell.reference.csv` |

So a paladin or mage that bubbles while carrying drops its puddle instantly, wherever it is standing -
and critical health arrives during a Tympanic Tantrum, which is the same window where the carrier is
still in the raid and cannot walk out. `XT002TargetGuardMultiplier` now zeroes both while the bot
carries either debuff. Searing Light (schoolMask 66) is in the same mask and drops its Life Spark the
same way, so the gate covers both.

### Damage numbers

All base plus a die roll, no health-percentage component, no `spell_dbc` override, and no
`spell_bonus_data` row. The triggered burst is cast by the **player**, since
`SpellInfo::NeedsToBeTriggeredByCaster` returns false for it, which is also why
`SPELL_ATTR1_EXCLUDE_CASTER` on 63025/64233 leaves the carrier out of its own burst.

| Effect | 10-man | 25-man | Radius |
|---|---|---|---|
| Burst on the carrier's allies (63025 / 64233) | 11,700 - 12,300 | 14,625 - 15,375 | 12 yd |
| Pull towards the carrier, same spell effect 2 | 140 | 140 | 20 yd |
| Single tick on the carrier itself (63024 / 64234 effect 3, period 9000 ms) | 12,000 flat | 15,000 flat | self |
| Consumption per tick (64208) | 900 - 1,100 | 900 - 1,100 | 5 yd |
| Searing Light per tick (63023 / 65120) | 2,250 | - | 8 yd |

All shadow except Searing Light, which is holy plus arcane. The apparent one-shots are the 15k burst
landing on a ranged stack that shares one anchor inside 5 yd of each other, on top of Tympanic Tantrum
ticks, with the pull then dragging survivors into the puddle that just spawned. Our clearances sit
above the burst either way: a parked carrier is 30-50 yd out, and the stop-short point is gated on
20 yd, the pull radius, which already covers the 12 yd damage radius.
