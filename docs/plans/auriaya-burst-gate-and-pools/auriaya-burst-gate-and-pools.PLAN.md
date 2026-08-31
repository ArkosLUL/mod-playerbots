# Auriaya: open the burst gate, contain the pools

**State:** nothing implemented. Working tree clean at `339e7ca02`; every line number below was taken
at that commit.

## Context

Two defects came out of the same live pull. The kill succeeded, so both are throughput bugs, not
survival bugs.

**A. No burst cooldowns fired at all.** Not Bloodlust, not potions, not trinkets, not a single class
cooldown. `UlduarBurstWindowMultiplier` zeroes every burst action for the whole Auriaya encounter
because **Sara, the idle Yogg-Saron phase-1 NPC, is 114 yards away and the multiplier's Yogg branch
searches 200 yards**. Not Auriaya-specific — General Vezax sits 167 yd from Sara and is blocked the
same way.

**B. Bots pace instead of fighting once pools land.** The anchored-Auriaya rework (commit
`9ea4242da`, "Auriaya: soak Sonic Screech and anchor the fight"; its plan was folded into
`docs/raids/ulduar.md` by `3b5f9a4de`) fixed the Sonic Screech soak and stopped bots wandering into
the corridor, but with 2+ Seeping Feral Essence pools near the raid they walk instead of casting.

The Feral Defender carries `SPELL_RANDOM_AGGRO_PERIODIC (61906)` — untankable, it pounces a random
raider and dies there. Pools land **in the stack** by design; no off-tank work can place them
elsewhere, so containment has to happen on our side.

Parts A and B are independent. A is the smaller change and the larger win.

---

# Part A — the burst gate

## The failure chain

Every link verified against the DBC extract, the world DB and the live container. Do not re-derive.

1. `RaidUlduarStrategy::InitMultipliers` installs `UlduarBurstWindowMultiplier` on every bot for the
   whole instance (`UldStrategy.cpp:856`).
2. `EvaluateWindow` sweeps `possible targets no los` — `sPlayerbotAIConfig.sightDistance = 100.0`
   (`ValueContext.h:445-448`, `playerbots.conf:599`, no `AC_*` override). At Auriaya it finds
   nothing: the nearest gated boss is Molgeim at 371 yd.
3. The last branch before the permissive fall-through is proximity to the Yogg-Saron encounter
   (`UldMultipliers.cpp:874-885`):
   ```cpp
   if (bot->FindNearestCreature(NPC_YOGG_SARON, 200.0f, true) ||
       bot->FindNearestCreature(NPC_SARA_PHASE_1, 200.0f, true))
   ```
4. **Sara (33134) is a static spawn** at `(1980.3, -25.6, 329.4)`, `spawnMask 3`, `phaseMask 1`,
   `spawntimesecs 604800` — and `Map::OnCreateMap` calls `LoadAllGrids()` for every map with an
   instance id (`src/server/game/Maps/Map.cpp:88-93`). She is alive, loaded and findable from the
   moment the instance is created, whether or not anyone has been near the Descent into Madness.
5. She is **114.2 yd** from Auriaya's spawn and ~115–130 yd from every bot anchor in that room —
   inside the 200 yd search, which is 3D. `Cell::VisitObjects` defaults to `dont_load`, but that
   costs nothing here because the grids are already loaded.
6. Yogg-Saron himself has **no static spawn** (summoned at the P2 transition), so
   `YoggSaronInPhase3()` and `YoggSaronInPhase2()` both return false
   (`UldBossHelper.cpp:429-443`).
7. The branch returns `{phaseThree || phase2, phaseThree}` = `{false, false}`, so
   `allowAll = false, allowLust = false`, and `GetValue` returns `0.0f` for every name in
   `burstCooldownNames` (`BurstCooldowns.cpp:22-46`).

The rotation is untouched — the multiplier early-outs on any non-burst action — which is why the
fight was still a kill.

## Blast radius

3D distance from Sara to each encounter, from the `creature` table:

| Encounter | Distance to Sara | Effect |
|---|---|---|
| **Auriaya** | 114.2 yd | Burst dead for the whole fight |
| **General Vezax** | 166.9 yd | Same — burst dead for the whole fight |
| Hodir | 207.5 yd | Boss-to-Sara only; the check measures from the **bot**, so bots on the near side of the room dip under 200 and lose burst while others keep it |
| Kologarn | 218.6 yd | Same borderline case along the walkway |

## The fix

`src/Ai/Raid/Uld/UldMultipliers.cpp` — `UlduarBurstWindowMultiplier::EvaluateWindow`, the Yogg
branch at `:874-885`.

Require the encounter to be **live**, not merely present: keep the two `FindNearestCreature` lookups
but bind each result and accept the branch only when that creature `IsInCombat()`.

- Sara is out of combat until `JustEngagedWith` runs `SetInCombatWithZone()`
  (`src/server/scripts/Northrend/Ulduar/Ulduar/boss_yoggsaron.cpp:549`), so an idle Sara no longer
  closes the gate.
- In P2/P3 Yogg is summoned and in combat, so the branch still applies where it should.
- Drop the radius to `sPlayerbotAIConfig.sightDistance` as well. The in-combat check is the real
  fix; the radius is belt-and-braces, and it makes this branch consistent with every other one.

Replace the comment. It currently explains the 200 yd figure as covering Sara's presence before the
P2 summon. Say instead that presence alone is not the encounter — Sara stands in her prison from
instance creation and is in range of two other bosses' rooms.

## Verification (Part A)

**Static:** no `FindNearestCreature(... 200.0f ...)` left in `EvaluateWindow`; the branch reads
combat state on both lookups.

**In game — the whole point of the change:**
1. Pull Auriaya with a shaman and offensive potions in the DPS bots' bags. Bloodlust and potions
   should fire once a tank has held a boss-flagged target for the dwell. Neither fired at all before.
2. Pull Vezax. Same check — this is the second encounter the bug covers.
3. Pull Yogg-Saron and confirm the gate still **closes**: no lust in P1, no lust until phase 3.

## Known residual, deliberately not fixed here

Once the gate opens, the framework's `HoldBurstUntilTankEngagedMultiplier` still keys its dwell on
the **bot's own current target** (`BurstWindowStrategy.cpp:24-91`). On Auriaya that is usually the
Feral Defender, which has random aggro, so its victim is never a tank:

- Sanctum Sentry (34014) and Feral Defender (34035) both carry `type_flags = 108`, which includes
  `CREATURE_TYPE_FLAG_BOSS_MOB (0x4)`, so `IsBossCreature` passes and the lust "ask the main tank
  instead" redirect at `BurstWindowStrategy.cpp:56-60` never fires.
- `AuriayaSetDpsPriorityAction` puts every ranged bot on the Defender whenever it is up. It spawns
  60 s in with 8 `SPELL_FERAL_ESSENCE` stacks (9 lives) and is down 38 s per death
  (`boss_auriaya.cpp:322, 353, 411-428`), so the windows where a bot sits on Auriaya long enough to
  satisfy the 3 s / 5 s dwell are those gaps only.

Expect lust and potions in the Defender-down windows, not at the pull. Tightening that is a
framework change touching every encounter — raise separately.

---

# Part B — pool containment

## Established facts

| Fact | Source |
|---|---|
| **The pool's real damage radius is 5 yd, not 10.** Stalker 34098 carries aura 64458 (effect 6, aura 23, period 1000ms), which triggers **64459** — effect 2 school damage, `EffectRadiusIndex_1 = 8` → **radius 5, max 5** | `creature_template_addon.auras` for 34098; `spell.reference.csv` id 64458/64459; `spellradius.reference.csv:8` |
| Pool damage is ~4,500/tick, once per second | `spell.reference.csv` id 64459, `EffectBasePoints_1 = 4499`, `EffectDieSides_1 = 1` |
| No `spelldifficulty` row for 64457/64458/64459 — **same radius in 10 and 25** | `spelldifficulty.reference.csv` |
| Pools are permanent (no duration, nothing despawns them), up to 9 per pull | `docs/raids/ulduar.md` |

DBC values come from `modules/mod-spell-tweaks/data/dbc-reference/*.csv`, not the world DB.

## The four faults

1. **Pool radius is 2× reality.** `ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS = 10.0f`
   (`UldBossHelper.h:2056`) was an explicit guess — "radius is DBC, so 10 yd is a conservative
   guess". The real value is 5. That is 4× the exclusion area: bots dodge pools they are not standing
   in, and two pools foul a 20 yd band instead of 10.

2. **The 12 yd leash is abandoned exactly when it matters.** When the 8×3 yd sweep finds no candidate
   clear of every pool, `AuriayaSeepingEssenceAction::Execute` falls through to
   `FleePosition(nearest->GetPosition(), SEEPING_ESSENCE_RADIUS, 500)`
   (`UldActions_Auriaya.cpp:134-151`), which respects neither the leash nor the anchor. This is the
   run across the room.

3. **Anchor and dodge ping-pong.** `AuriayaRaidPositionTrigger` stands down only *while* the essence
   trigger is live (`UldTriggers_Auriaya.cpp:48-51`). So: dodge succeeds → essence trigger goes quiet
   → anchor trigger fires → bot is walked straight back onto a fouled anchor → dodge again. Forever.
   Each loop also returns `true` at `ACTION_RAID + 2`, and `Engine::DoNextAction` breaks at the first
   action returning `true`, so a looping bot starves its own rotation too.

4. **Station retirement cannot see where the raid stands.** `AuriayaStationClear` tests two fixed
   points per station with a 12 yd radius (`UldBossHelper.cpp:507-519`). Bots legitimately occupy a
   disc of ~20 yd around the anchor (5–8 yd arrival tolerance plus the 12 yd dodge leash), so pools
   that make the ground unusable routinely fall outside the test and the fight never relocates.

## Design decisions

Settled with the user.

1. **Pool radius → 7 yd.** Real 5 plus 2 yd for tick granularity and position lag.
2. **The leash is absolute.** No path may move a bot more than `ESSENCE_LEASH` from its anchor. When
   nothing inside the leash is fully clear, take the **least bad** point rather than fleeing.
3. **The single-point anchor stays.** No arc or wedge — the radius fix plus relocation is expected to
   supply enough clear ground, and a pool-aware anchor would make the trigger and action recompute
   the same pool set twice per tick.
4. **Retirement becomes a score, not a boolean.** Count the pools fouling each station and take the
   lowest count, ties to the lowest index.

   Rationale, and a deliberate departure from the literal "test the live anchor" instruction: any
   retirement input derived from the **boss's current position** oscillates, because the boss follows
   the MT and the MT follows the retirement verdict. Retire station 0 → MT walks to station 1 → boss
   follows → the live anchor is now clean → station 0 un-retires → MT walks back. Pool **counts**
   against fixed geometry only ever grow, so the index slides west monotonically and can never
   reverse. Widening the radius to the raid's true footprint is what recovers the information the
   live test was wanted for.

## Changes

### 1. `src/Ai/Raid/Uld/Util/UldBossHelper.h`

Replace the guessed radius at `:2056` and its comment:

```cpp
// 64458 on the stalker is a 1s periodic trigger of 64459, whose radius index (8) is 5 yd in both
// difficulties. Two yards on top for tick granularity and position lag.
constexpr float ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS = 7.0f;
```

Replace `ULDUAR_AURIAYA_STATION_CLEAR_RADIUS` at `:2077` with:

```cpp
constexpr float ULDUAR_AURIAYA_STATION_FOUL_RADIUS = 15.0f;
```

15 yd is the raid's real footprint around a station point: up to 8 yd of arrival tolerance plus the
pool radius. Note in the comment that the metric is a **count**, and that counts are used precisely
because they cannot decrease while a station is occupied.

`ULDUAR_AURIAYA_ESSENCE_LEASH` (`:2083`, 12 yd) is unchanged.

### 2. `src/Ai/Raid/Uld/Util/UldBossHelper.cpp`

Delete `AuriayaStationClear` (`:507-519`). Rewrite `GetAuriayaStationIndex` (`:521`) as a single
scoring pass:

- For each station `i`, count pools within `ULDUAR_AURIAYA_STATION_FOUL_RADIUS` of **either**
  `ULDUAR_AURIAYA_MAINTANK_SPOTS[i]` **or** `ULDUAR_AURIAYA_NOMINAL_RAID_POINTS[i]`.
- Return the lowest count; ties go to the lowest index.
- This subsumes the existing "all stations polluted → greatest clearance" fallback, so that branch
  goes away with it.

Keep `CollectAuriayaEssencePools` and the boss-null guard unchanged.

### 3. `src/Ai/Raid/Uld/Action/UldActions_Auriaya.cpp` — `AuriayaSeepingEssenceAction::Execute`

Replace the two-outcome search (clear candidate, else `FleePosition`) with one scored search that
always terminates inside the leash:

- Candidate set: the existing 8 directions × 3 yd out to `ESSENCE_LEASH`, **plus the bot's current
  position and the anchor itself**, each filtered by `anchor.GetExactDist2d(...) <= ESSENCE_LEASH`
  and `bot->IsWithinLOS(...)`.
- Score, in order: fewest pools within `SEEPING_ESSENCE_RADIUS`; then greatest distance to the
  nearest pool; then smallest displacement from the bot's current position.
- Return `false` when the winner is the bot's current position — it is already standing in the best
  spot available, so hand the tick to the rotation.
- Delete the `FleePosition` fallback and the `nearest` scan that feeds it.

Comment the invariant: a bot never leaves the leash, even when every point inside it is fouled.

### 4. `src/Ai/Raid/Uld/Trigger/UldTriggers_Auriaya.cpp` — `AuriayaRaidPositionTrigger::IsActive`

Add a second stand-down: after resolving the anchor, return `false` when any pool sits within
`ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS` of the anchor itself. Without it the anchor node walks bots
back into a pool every time the dodge succeeds. Keep the existing "essence trigger is live"
stand-down at `:48-51` — it covers the frames before the dodge has moved anyone.

### 5. `docs/raids/ulduar.md`

**Invoke `/compact-docs-writer` before editing.** Auriaya section starts at `:1380`, Vezax at
`:1531`.

Update Auriaya: the pool radius is 5 yd DBC-verified (not a guess), ~4,500/sec, identical in both
difficulties; station selection is a pool count over a 15 yd footprint; the dodge is leash-absolute.
Record the oscillation finding — that any boss-position-derived retirement input feeds back through
the MT — since it is expensive to rederive and invites the same wrong fix later.

Add the Sara proximity trap from Part A to both the Auriaya and Vezax sections: an idle static NPC
114/167 yd away silently closed the burst gate. It costs a whole session to rediscover.

## Verification (Part B)

The module cannot be compiled in this environment. Static checks here; build and in-game by the user.

**Static:**
1. `grep -rn "FleePosition" src/Ai/Raid/Uld/Action/UldActions_Auriaya.cpp` → nothing.
2. `grep -rn "STATION_CLEAR_RADIUS\|AuriayaStationClear" src/` → nothing left behind.
3. `ULDUAR_AURIAYA_STATION_FOUL_RADIUS` declared once, used only in `GetAuriayaStationIndex`.
4. Every candidate path in the essence action is guarded by the leash check — no `MoveTo` reachable
   without it.

**In game:**
1. Kill the Defender once in the stack. Bots should step a few yards clear and **hold**. Nobody
   should travel more than ~12 yd from their spot at any point in the fight.
2. Let 3–4 pools accumulate on station 0. The MT should advance west one station and the raid should
   follow the boss without reshuffling. Confirm it **never walks back east** as pools keep dropping.
3. Stand a bot where every point within its leash is fouled. It should settle on the least-damaging
   spot and keep casting, not run.
4. Confirm the anchor node never pulls a bot into a pool: watch one bot dodge, then verify it stays
   put rather than immediately returning.
5. Sanity-check the radius change: a bot 8 yd from a pool should not move at all.

---

## Follow-up

- **Second proximity leak, same shape.** Freya sits 136.5 yd from Elder Ironbranch, 143.8 from Elder
  Stonebark and 190.1 from Elder Brightleaf, and her branch fires on presence within the 100 yd
  sweep. A bot fighting an Elder from the Freya-facing side loses its burst. The same in-combat
  guard fixes it — verified safe for Thorim, whose gauntlet trash calls
  `thorim->SetInCombatWithZone()` (`boss_thorim.cpp:1074-1076`), so the balcony gate stays shut.
  Mimiron needs its own look first: the branch currently keys on all three mechs being *alive*, and
  the idle ones may already be in range during P1.
- **`use trinket` has no role carve-out in the Ulduar multiplier.** The framework version exempts it
  for non-DPS (`BurstWindowStrategy.cpp:36-39`) precisely because it also covers survival and mana
  trinkets; `UlduarBurstWindowMultiplier` zeroes it for everyone. On every gated Ulduar boss, tanks
  and healers lose their trinkets. Same for `avenging wrath` / `power infusion` on healers, which
  the framework exempts at `:41-44`.
- The dwell keyed on the bot's own current target, described under Part A's residual.
