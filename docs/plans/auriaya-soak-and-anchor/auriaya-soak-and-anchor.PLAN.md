# Auriaya: soak the Screech, anchor the fight, contain the pools

## Context

Bots kill Auriaya but spend the fight walking. They drift east out of her chamber and dangle in the
corridor instead of holding position and casting.

Two structural causes:

1. **The Sonic Screech handling is inverted.** Sonic Screech splits its damage among everyone it
   hits, so the [WarcraftTavern guide](https://www.warcrafttavern.com/wotlk/guides/auriaya-strategy-guide-ulduar-25/)
   and Icy Veins both say to face her *at* the raid. Our strategy does the opposite twice over: every
   non-victim bot sidesteps out of the 120° arc, and the main tank arc-steps her so she faces *away*
   from the raid centroid. She faces her victim, so the MT sits permanently inside her own cone and
   soaks the entire unsplit hit alone. At 25-man that hit is **190,000–210,000** — a guaranteed tank
   death every 50s, not a survivable mistake.
2. **Nothing anchors anyone.** Every Auriaya movement is emergent, and two of them form feedback
   loops with no fixed point to converge on. The void-zone flee in particular walks a bot up to 30 yd
   in whatever direction is farthest from a pool, which — as permanent pools accumulate — is reliably
   up the corridor.

The fix follows the XT-002 anchored-positioning work already merged on `Custom`
(`docs/plans/xt002-anchored-positioning/xt002-anchored-positioning.PLAN.md`), with one deliberate
departure: XT-002 never moves, so it can anchor everything to world coordinates. Auriaya walks to her
victim, so only the **main tank** gets fixed spots and everyone else anchors relative to the boss.

Net node count: **−2 triggers, −1 action.**

## Established facts

Verified this session against the DB, the DBC extract and the core script — do not re-derive.

| Fact | Source |
|---|---|
| Sonic Screech **splits damage**: `spell_custom_attr` `(64422, 32776)` = `SHARE_DAMAGE` \| `IGNORE_ARMOR`, `(64688, 8)` = `SHARE_DAMAGE` | `data/sql/base/db_world/spell_custom_attr.sql:391,398`; `SpellInfo.h:180,192`; split applied at `SpellEffects.cpp:3698` |
| Damage: **64422 = 60,125–69,875**; **64688 = 190,000–210,000**. Radius 80 yd | `modules/mod-spell-tweaks/data/dbc-reference/spell.reference.csv:43803,44009`; radius index 31 → `spellradius.reference.csv:26` |
| Cone is **120°** for both difficulties | `data/sql/updates/db_world/2026_06_11_02.sql:513,522` |
| **No SpellScript for Sonic Screech** — pure `DoCastSelf`, so DB attributes are the whole truth | `boss_auriaya.cpp:214-217` |
| Auriaya spawns at **`(1956.2, 49.3155, 411.358)`, `o = 2.84121`** (guid 137496, entry 33515, map 603) | `data/sql/base/db_world/creature.sql:129538` |
| `o = 2.84121` → facing unit vector **`(-0.95524, 0.29588)`**, pointing **away from the entrance** | derived |
| Chamber floor **z ≈ 411.35–412.4, x 1909→1956, y 43→82**; the corridor is at **+x, raised to z 417.7** | neighbouring spawns `creature.sql:128937-129608`; travel node `playerbots_travelnode.sql:887` |
| **Seeping Feral Essence pools are permanent.** Spell 64457 has `DurationIndex 21` → duration −1; no SmartAI rows for 34098; the Defender's 30s respawn path never calls `DespawnAll`. Up to **9 per pull** | `spell.reference.csv:43829`; `spellduration.reference.csv:22`; `TemporarySummon.cpp:209-213`; `boss_auriaya.cpp:318,332-335,350,361,411-432` |
| Defender carries `SPELL_RANDOM_AGGRO_PERIODIC (61906)` — untankable, so taunt-and-drag cannot park its pools | `boss_auriaya.cpp:323,428` |
| **`AttackAction : MovementAction`** — a blanket `dynamic_cast<MovementAction*>` zero also kills targeting. `DpsAssistAction` and `AttackRtiTargetAction` both sit under it | `AttackAction.h:16`; `ChooseTargetActions.h:22,76` |
| **`CastReachTargetSpellAction` is *not* a `MovementAction`** — it derives from `CastSpellAction` and only gates on distance. The action that actually walks a bot into range is `ReachTargetAction`/`ReachSpellAction` | `ReachTargetActions.h:15,31,51` |
| `nearest npcs` is **bot-centred, 100 yd, LOS-filtered, alive-filtered**, and *does* include `UNIT_FLAG_NOT_SELECTABLE` stalkers. Cached 1/sec | `NearestNpcsValue.cpp:16-21`; `NearestUnitsValue.cpp:18`; `PlayerbotAIConfig.cpp:112` |
| `GetCreatureListWithEntryInGrid` applies **no** alive/LOS/selectability filter — callers must add their own | `Object.cpp:2614-2619`; `GridNotifiers.h:1504-1520` |
| `Engine::DoNextAction` breaks on the first action returning `true` — node priority starves, it does not merely order | `src/Bot/Engine/Engine.cpp:219-225` |
| `PlayerbotAI::IsRanged(p)` counts healers; `IsRangedDps` excludes them | `PlayerbotAI.cpp:1905`; `PlayerbotAI.h:460` |
| Cadences: Sonic Screech 45s then 50s; Terrifying Screech 35s; Sentinel Blast 35s (delays other events 5s); Feral Defender at 60s | `boss_auriaya.cpp:160-165,209-226` |

## Analysis: guide vs. what we ship

| Guide instruction | Current behaviour | Verdict |
|---|---|---|
| Burn the 4 Sanctum Sentries first — Strength of the Pack (64369) buffs her while they live | `GetAuriayaFocusTarget` returns sentries first | ✅ |
| Hold sentries in melee (Savage Pounce only fires at 8–25 yd) | `auriaya sentry taunt` on assist tank 0 | ✅ |
| Break the Terrifying Screech fear | Fear Ward / Tremor Totem via `RaidAntiFear`, whole fight as one window | ✅ |
| **Face her at the raid so Sonic Screech splits** | Cone dodge for everyone + MT turns her away from the raid | ❌ **inverted** |
| Keep the fight in one place, pools out of the way | No anchor anywhere; unbounded 30 yd flee | ❌ **missing** |
| Sentinel Blast is a raid-wide heal check, not a dodge | Correctly left to healing | ✅ |

### Why bots end up in the corridor

**A. The tank-facing loop never converges.** `GetAuriayaRaidCentroid` averages live non-tank
positions; the MT arc-steps 0.125 rad/tick at `MOVEMENT_FORCED` onto the "raid → boss" bearing. Each
step turns the boss, sweeping the 120° cone across the raid; every bot the cone touches rotates to
`cone/2 + 15°`, which moves the centroid, which re-opens the facing error. Tank chases raid, raid
chases cone, forever.

**B. The cone dodge is not gated on the cast.** `AuriayaSonicScreechTrigger` fires whenever the bot
sits in the arc within 45 yd — the spell is up for 2.5s of every 50s. At `ACTION_RAID + 1` it beats
the targeting node, so roughly a third of the raid is walking rather than casting at any instant.

**C. The void-zone flee is unanchored and maximises the wrong thing.**
`MoveAwayFromCreatureAction::Execute` sweeps 8 directions × 3 yd out to **30 yd** and picks the
candidate with the *greatest* distance to the nearest pool (`MovementActions.cpp:2865-2910`). With
permanent pools accumulating, that maximum is always outward — down the corridor. Once out there the
trigger goes false, the bot walks back, re-enters 10 yd of a pool, and flees again.

**D. The whole raid chases a randomly-aggroing add.** Once the sentries die,
`GetAuriayaFocusTarget` promotes the Feral Defender to skull and every non-MT bot follows it via
`attack rti target`. It re-rolls its target periodically and roams, dragging melee with it.

## Design decisions

Settled with the user across three grilling rounds. Reasoning kept where it is not obvious.

1. **Soak, don't dodge.** Delete the cone dodge and the tank facing. Anchoring the MT *is* the facing
   control: she faces her victim, so a stationary MT means a stationary cone, and the raid stacks on
   that bearing to soak it. Removes the A/B feedback loop as a side effect of following the guide.
2. **Hybrid anchor frame.** The MT anchors to fixed world spots; everyone else anchors to
   `boss position + 20 yd along the boss→MT bearing`. Fixed coordinates are good for exactly one
   thing — steering the fight away from the corridor — and bad at surviving a human tank or a knocked
   tank. Boss-relative anchoring is self-correcting and needs no coordinates. The hybrid takes both.
3. **Quantize the bearing to π/16 (11.25°).** MT jitter would otherwise propagate to 20 bots.
   Quantization is stateless and deterministic, so no two bots can desync, and at a 20 yd radius one
   bucket is a 3.9 yd shift — under the 5 yd arrival tolerance, so bucket flicker never moves anyone.
   Advancing a station moves the MT *along* the lane, so the bearing does not rotate at all.
4. **Soakers are MT + ranged DPS + healers. Melee stay behind her and sit out.** Pinning melee in the
   arc costs positional attacks all fight for a share the other soakers already make small: 10N is
   ~65k over 6–7 ≈ 10k each, 25N is ~200k over ~17 ≈ 12k each. **No minimum-soaker floor** — a
   conditional cone-dodge would reintroduce exactly the machinery causing the corridor dance.
5. **Healers are anchored**, unlike XT-002. There the ranged spot sat ~30 yd from the tank with no
   slack; here the stack is 20 yd from the boss, well inside heal range of the MT and the melee pile.
   Dropping them would cost ~6 of 17 soakers at 25-man, taking each survivor from ~12k to ~18k. Give
   them a looser tolerance and spare `ReachTargetAction` so they can still close on a real heal.
6. **Three stations, 10 yd apart, along her home facing.** Permanent pools mean the fight has to
   relocate. Three is what the confirmed floor supports; a fourth would put the stack at x ≈ 1908,
   past anything verifiable.
7. **Only the MT computes the station index.** Everyone else inherits the advance through the
   bearing, so there is no cross-bot agreement problem and the LOS-filtered pool set cannot desync
   anything. **No fallback when a human tanks** — ranged and healers still anchor correctly off
   wherever the human stands, which is what the hybrid bought; only automatic relocation is lost.
8. **The essence dodge minimises displacement, not pool distance**, and is leashed to the bot's own
   anchor. Direct fix for C: step just far enough to clear, never wander.
9. **The movement guard must spare `AttackAction` and `ReachTargetAction`.** XT-002's version zeroes
   every `MovementAction` for ranged DPS, which silently also zeroes `AttackRtiTargetAction` despite
   an adjacent comment promising a human's mark still wins. Do not copy that bug.
10. **Drop RTI, keep obeying it.** Bots stop setting the skull — icons are group-global and stamp
    over the player's marks; `attack rti target` stays registered and live.
11. **Melee take the Feral Defender only within 15 yd of the boss.** Cleave it when it walks into the
    pile, never leave her to chase it around the room.
12. **`MOVEMENT_COMBAT`, no teleport branch.** One flat room; `MOVEMENT_FORCED` would outrank the
    dodge, the one thing that must never happen.

## Station geometry

Lane bearing is her home facing, `f = (-0.95524, 0.29588)` — directly away from the corridor at +x.

| Station | MT spot (home + (5+10k)·f) | Nominal raid point (home + (20+10k)·f) |
|---|---|---|
| 0 | `(1951.42, 50.79, 411.36)` | `(1937.10, 55.23, 411.36)` |
| 1 | `(1941.87, 53.75, 411.36)` | `(1927.54, 58.19, 411.36)` |
| 2 | `(1932.32, 56.71, 411.36)` | `(1917.99, 61.15, 411.36)` |

All six sit inside the observed floor envelope. Z is the boss's own; `MoveTo` resolves navmesh
height. The nominal raid points exist **only** for the station retirement test — the raid's actual
anchor is always derived from the live boss and MT.

**Retirement:** a station is dead when a pool sits within `STATION_CLEAR_RADIUS` (12 yd, i.e. 2 yd of
margin over the 10 yd essence radius) of its MT spot **or** its nominal raid point. In practice the
raid-point test fires first, since the Defender dies where it aggros.

**Selection:** lowest index not retired. If all three are retired, fall back to the station whose
*nearest* pool is farthest away. Pools are permanent, so this metric only changes when a new pool
spawns — it cannot oscillate.

## Files to change

### 1. `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp`

**Remove** — no callers left:
- `GetAuriayaRaidCentroid` (`.cpp:451`), `GetAuriayaFacingError` (`.cpp:481`), and their declarations.
- `ULDUAR_AURIAYA_SONIC_SCREECH_CONE`, `ULDUAR_AURIAYA_SONIC_SCREECH_RANGE`,
  `ULDUAR_AURIAYA_FACING_TOLERANCE`, `ULDUAR_AURIAYA_FACING_ARC_STEP`,
  `ULDUAR_AURIAYA_FACING_MIN_RAID_DIST` (`.h:700-709`).

Leave `IsBotInFrontalCone` / `GetPositionOutsideFrontalCone` in `RaidBossHelpers` alone — other
encounters use them.

**Add**, beside the existing `ULDUAR_XT002_*` spots (`.h:760-762`, `.cpp:64-70`):

```cpp
constexpr int   ULDUAR_AURIAYA_STATION_COUNT           = 3;
constexpr float ULDUAR_AURIAYA_STATION_CLEAR_RADIUS    = 12.0f;
constexpr float ULDUAR_AURIAYA_RAID_STANDOFF           = 20.0f;   // raid stack distance from the boss
constexpr float ULDUAR_AURIAYA_BEARING_QUANTUM         = float(M_PI) / 16.0f;
constexpr float ULDUAR_AURIAYA_MAINTANK_SPOT_TOLERANCE = 3.0f;
constexpr float ULDUAR_AURIAYA_RANGED_SPOT_TOLERANCE   = 5.0f;
constexpr float ULDUAR_AURIAYA_HEALER_SPOT_TOLERANCE   = 8.0f;
constexpr float ULDUAR_AURIAYA_ESSENCE_LEASH           = 12.0f;   // max drift from the bot's anchor
constexpr float ULDUAR_AURIAYA_MELEE_DEFENDER_RANGE    = 15.0f;   // melee cleave gate
```

plus `extern const Position ULDUAR_AURIAYA_MAINTANK_SPOTS[]` and
`ULDUAR_AURIAYA_NOMINAL_RAID_POINTS[]`.

**Add** helpers:

- `std::vector<Unit*> CollectAuriayaEssencePools(WorldObject* from)` — `GetCreatureListWithEntryInGrid`
  for 34098 at 100 yd with an **explicit alive filter** (the grid searcher applies none). Same shape
  as `IccGetCreaturesByEntry` (`ICCShared.cpp:19-31`). Boss-centred for the station test, bot-centred
  for the dodge, one implementation for both — this also removes the existing mismatch where
  `AuriayaSeepingEssenceTrigger` uses `FindNearestCreature` while its action uses `nearest npcs`.
- `int GetAuriayaStationIndex(PlayerbotAI*)` — lowest non-retired index; if all retired, the index
  with the greatest minimum pool distance.
- `bool GetAuriayaAnchor(PlayerbotAI*, Player*, Position& out, float& tolerance)` — the single source
  of truth, called by both the trigger and the action (`docs/engine/raid-mechanics-lessons.md:131-132`):
  - **MT** → `ULDUAR_AURIAYA_MAINTANK_SPOTS[GetAuriayaStationIndex(...)]`, MT tolerance.
  - **`IsRangedDps`** → boss + `RAID_STANDOFF` along the quantized boss→MT bearing, ranged tolerance.
  - **other `IsRanged`** (healers) → same point, healer tolerance.
  - **anyone else** → `false`.
  - No bot MT → fall back to the boss's own facing for the bearing, so ranged still land in the arc
    of whoever is tanking.

### 2. `src/Ai/Raid/Uld/Trigger/UldTriggers_Auriaya.h` / `.cpp`

**Remove:** `AuriayaSonicScreechTrigger`, `AuriayaTankFacingTrigger`, `AuriayaMarkDpsTargetTrigger`,
`AuriayaAttackDpsTargetTrigger`, and the now-unused `RtiTargetValue.h` include.

**Add:**
- `AuriayaRaidPositionTrigger` — boss up; **false** when `AuriayaSeepingEssenceTrigger` would fire
  (the only stand-down: fear and stuns are already covered by `isPossible`/`CanFreeMove`, and the
  anchor must specifically *not* stand down during the Screech cast); then `GetAuriayaAnchor` and a
  distance-vs-tolerance check.
- `AuriayaSetDpsPriorityTrigger` — boss up, `!botAI->IsTank(bot)`.

**Change:** `AuriayaSeepingEssenceTrigger` to use `CollectAuriayaEssencePools(bot)`.

**Keep unchanged:** `AuriayaFallFromFloorTrigger`, `AuriayaSentryTauntTrigger`, `AuriayaAntiFearTrigger`.

### 3. `src/Ai/Raid/Uld/Action/UldActions_Auriaya.h` / `.cpp`

**Remove:** `AuriayaSonicScreechAction`, `AuriayaTankFacingAction`, `AuriayaMarkDpsTargetAction`.

**Add `AuriayaRaidPositionAction : MovementAction`** — `"auriaya raid position action"`. Resolve
`GetAuriayaAnchor`; inside tolerance → `return false` so the rotation runs on the same tick;
otherwise `MoveTo` at `MovementPriority::MOVEMENT_COMBAT`.

**Add `AuriayaSetDpsPriorityAction : AttackAction`** — `"auriaya set dps priority action"`. Copy the
shape of `MuruSetDpsPriorityAction` (`src/Ai/Raid/SWP/Action/SWPActions_Muru.cpp:178-393`): one pass
over `"nearest npcs"` building a `vector<pair<entry, Unit*>>`; priority **Sanctum Sentry (34014) →
Feral Defender (34035) → Auriaya (33515)**; sticky per entry (switch only when another candidate is
>10 yd closer) and across entries (keep the current target when its priority index is `<=` the
desired one); `needsAttack` guard returning `false` when already correct; fall back to
`AI_VALUE(Unit*, "dps target")`.

`isAllowedPriorityTarget` rules:
- **Feral Defender** — barred while `IsDownOrFeigning` (it feigns at 1 HP wearing
  `UNIT_FLAG_NOT_SELECTABLE`), and barred for melee unless it is within
  `ULDUAR_AURIAYA_MELEE_DEFENDER_RANGE` of the boss.

**Rewrite `AuriayaSeepingEssenceAction`** — stop deriving from `MoveAwayFromCreatureAction`; make it
a plain `MovementAction`:
- Anchor = `GetAuriayaAnchor` for anchored roles, else the boss's position (melee).
- Sweep 8 directions × 3 yd out to `ULDUAR_AURIAYA_ESSENCE_LEASH` from the **bot's current position**.
  A candidate is valid when it clears `ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS` from every pool, sits
  within the leash of the anchor, and passes `bot->IsWithinLOS`.
- Score = **smallest displacement from the bot's current position**.
- No valid candidate → `FleePosition(nearestPool, ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS, 500ms)`,
  which is navmesh-validated and carries `CheckLastFlee` anti-oscillation.
- Already clear → `return false`.

**Change `AuriayaFallFromFloorAction`** — teleport to the bot's own anchor via `GetAuriayaAnchor`
instead of to the master, who may be standing in the corridor. Fall back to the master when the
anchor cannot be resolved.

**Keep unchanged:** `AuriayaSentryTauntAction`, the anti-fear action.

### 4. Wiring — `UldActionContext.h`, `UldTriggerContext.h`, `UldStrategy.cpp`

Register two new triggers and two new actions; delete four trigger and three action registrations
(`UldTriggerContext.h:119-125, 250-256`; `UldActionContext.h:118-123, 246-251`). Replace the Auriaya
block at `UldStrategy.cpp:249-287`:

```cpp
"auriaya fall from floor trigger"     -> "auriaya fall from floor action"     ACTION_RAID + 4
"auriaya sentry taunt trigger"        -> "auriaya sentry taunt action"        ACTION_RAID + 3
"auriaya seeping essence trigger"     -> "auriaya seeping essence action"     ACTION_RAID + 2
"auriaya anti fear trigger"           -> "auriaya anti fear action"           ACTION_RAID + 2
"auriaya set dps priority trigger"    -> "auriaya set dps priority action"    ACTION_RAID + 1
"auriaya raid position trigger"       -> "auriaya raid position action"       ACTION_RAID
```

Taunt stays **above** the essence dodge, as today: the dodge is a step and delaying it a tick costs a
tick of damage, while a loose sentry costs the raid Strength of the Pack plus a pounce on a clothie.

Fall-from-floor moves from `ACTION_RAID` to `ACTION_RAID + 4` — it previously shared `ACTION_RAID`
with nothing, and would now tie with the anchor node; a bot under the floor can do nothing else
useful, so it takes the top.

Rewrite the block comment: state that Sonic Screech is deliberately **soaked**, not dodged, and that
position is the lowest node because the engine breaks on the first successful action — killing a
sentry beats standing on a spot.

### 5. `src/Ai/Raid/Uld/UldMultipliers.h` / `.cpp`

**Add `AuriayaMovementGuardMultiplier`** beside the existing anti-fear guard (`.h:141-151`), gated on
`GetAuriaya(botAI)`:

- Non-tanks: zero `dynamic_cast<DpsAssistAction*>(action)` — the priority action owns
  `current target`. Comment that `attack rti target` is deliberately *not* zeroed.
- **MT and `IsRanged` only**: zero `dynamic_cast<MovementAction*>(action)` unless it is also an
  `AttackAction`, or a `ReachTargetAction`, or its name is in
  `{"auriaya raid position action", "auriaya seeping essence action", "auriaya fall from floor action"}`.
  This catches `AvoidAoeAction`, `CombatFormationMoveAction` and `SetBehindTargetAction`, which would
  otherwise walk anchored bots off their spot every tick — the oscillation `RazorscaleMultiplier`
  exists to prevent (`UldMultipliers.h:63`). Assist tank 0 chases sentries and melee ride the boss, so
  both keep every generic mover; melee specifically need `SetBehindTargetAction` intact.
- Keep the `CastHealingSpellAction` escape hatch.

Register beside the anti-fear multiplier at `UldStrategy.cpp:642`.

The `AttackAction` / `ReachTargetAction` carve-outs are a deliberate divergence from
`XT002TargetGuardMultiplier::GetValue` (`UldMultipliers.cpp:115-126`), which has the latent bug. See
Follow-up.

### 6. `docs/raids/ulduar.md` and `docs/engine/pitfalls.md`

**Invoke `/compact-docs-writer` before editing.** Rewrite the Auriaya section at `:479-514`, which
documents the inverted design as correct. Fold in the facts table — the `SHARE_DAMAGE` attribute, the
real 25-man damage, and pool permanence are all expensive to re-derive.

`docs/engine/pitfalls.md` gets the two class-hierarchy traps: `AttackAction : MovementAction`, and
`CastReachTargetSpellAction` *not* being a `MovementAction` while `ReachTargetAction` is. Both bite
any multiplier that zeroes movement by `dynamic_cast`.

## Verification

The module cannot be compiled in this environment — static checks here, build and in-game by you.

**Static:**
1. `grep -rn "rti\|Rti\|RTI\|skull" src/Ai/Raid/Uld/Action/UldActions_Auriaya.* src/Ai/Raid/Uld/Trigger/UldTriggers_Auriaya.*` → nothing.
2. `grep -rn "GetAuriayaFacingError\|GetAuriayaRaidCentroid\|AuriayaSonicScreech\|AuriayaTankFacing\|AuriayaMarkDpsTarget\|AuriayaAttackDpsTarget" src/` → nothing left behind.
3. Every `creators[...]` string in `UldActionContext.h` / `UldTriggerContext.h` matches a
   `NextAction(...)` / `TriggerNode(...)` string in `UldStrategy.cpp` character for character — a
   mismatch is silent at runtime.
4. Every name in the multiplier allowlist matches a registered action name.
5. `CollectAuriayaEssencePools` has an explicit `IsAlive()` filter — the grid searcher has none.
6. New `Position` arrays declared `extern` once in the header, defined once in the `.cpp`.

**Build:** no new warnings — several classes change base class.

**In game:**
1. Pull her. The MT settles at `(1951.4, 50.8)` and **stops**; ranged and healers form a loose stack
   ~20 yd from her on the far side of the MT; melee ride her. Nobody paces.
2. Watch a Sonic Screech land: it should hit the MT plus every ranged and healer for a small, roughly
   equal amount. One player eating the whole hit means the bearing is wrong. In 25-man this is the
   difference between ~12k each and a dead tank.
3. Confirm nobody leaves the chamber — no bot should reach x > 1960 or z > 415 during the fight.
4. Kill the Defender once → one pool. Bots should step **just** clear and hold, not run 30 yd. Kill it
   until a station's spots are polluted → the MT advances one station west, the boss follows, and the
   raid slides with her without reshuffling (the bearing does not rotate on a lane-aligned step).
5. Terrifying Screech: Fear Ward on the tank, Tremor down, and bots return to their anchors after each
   fear rather than resuming from wherever they were feared to.
6. **No raid icon is ever set by a bot.** Then mark a skull manually mid-fight and confirm bots honour
   it — including ranged DPS, which is what the `AttackAction` carve-out buys.
7. Healers: confirm one whose target walks out of range actually moves, which is what the
   `ReachTargetAction` carve-out buys.
8. Melee never walk away from Auriaya to chase the Defender, but do swing at it when it comes to them.
9. Sentries: assist tank 0 taunts each loose one and holds it in melee; nothing takes Savage Pounce.
10. Tank with a human and confirm ranged still stack in the arc behind them; only station advance
    should be lost.

## Follow-up

- Copy this plan to `docs/plans/auriaya-soak-and-anchor/auriaya-soak-and-anchor.PLAN.md` per the
  project's planning-directory convention.
- `XT002TargetGuardMultiplier::GetValue` (`UldMultipliers.cpp:115-126`) zeroes every `MovementAction`
  for ranged DPS, which also zeroes `AttackRtiTargetAction` — despite the adjacent comment promising a
  human's mark still wins — and `ReachSpellAction`. The same two carve-outs fix it. Separate change.
- Upstream data asymmetry, not ours to fix: `spell_custom_attr` gives 64422 `IGNORE_ARMOR` but 64688
  does not.
- The guide's "melee step to her front to soak" refinement is deliberately not implemented, per
  design decision 4.
- Crazy Cat Lady (no sentry killed) stays incompatible with the kill order, unchanged.
