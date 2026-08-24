# Thorim phase 1: dodge the Charge Orb, and stop one Stormhammer debuffing the whole arena

## Context

Two more 25-man Thorim attempts wiped in phase 1, with the arena squad dying before the gauntlet
squad reached the Rune Giant. Traces:

- `env/dist/logs/botobs/603_3_thorim_1787600518.ndjson` — wipe at 2:59. Split held (15 arena / 10
  gauntlet). Arena emptied at 2:50; the gauntlet squad was at the Ancient Rune Giant when
  `boss_thorim.cpp` found the arena empty and summoned the Lightning Orb, which killed all 11 of them
  in 1.4 s. The arena needed roughly 30 more seconds.
- `env/dist/logs/botobs/603_3_thorim_1787597345.ndjson` — wipe at 3:21, no split at all, whole raid
  in the arena for three minutes.

Read with `python modules/mod-playerbots/tools/botobs/postmortem.py <file> [--bot NAME|--track NAME|--notes thorim.]`.

The previous round of fixes is live and working: `thorim dps priority action` runs, nobody attacks
Thorim on the balcony, the squad split is now visible in the trace as `thorim.squad` (1 = Arena,
2 = Gauntlet). What is left is two boss mechanics the module has never known about.

### Mechanic 1 — Charge Orb, never dodged

Verified from DBC (`modules/mod-spell-tweaks/data/dbc-reference/spell.reference.csv`) and
`src/server/scripts/Northrend/Ulduar/Ulduar/boss_thorim.cpp`:

- `EVENT_THORIM_CHARGE_ORB` fires 14 s into phase 1 and repeats every 16 s. It casts **Charge Orb
  62016** with `SPELLVALUE_MAX_TARGETS 1`; `conditions` row `(13,1,62016,…,33378)` restricts the
  target to a **Thunder Orb (33378)**.
- 62016 is `SPELL_EFFECT_APPLY_AURA` / `SPELL_AURA_PERIODIC_TRIGGER_SPELL`, period **1000 ms**,
  duration **15 000 ms**, trigger spell **62017**. So the orb carries aura 62016 for the whole window.
- **Lightning Shock 62017**: `SPELL_EFFECT_SCHOOL_DAMAGE`, base points 2830 + die 339 →
  **~2831-3170 nature per tick**, targets `TARGET_SRC_CASTER` + `TARGET_UNIT_SRC_AREA_ENEMY`,
  `EffectRadiusIndex 21` = **35 yd**. Not dispellable, no cast bar of its own.
- There are **7 Thunder Orbs**, fixed spawns, all at **z 433.3** and all **42.0 yd** from the arena
  centre (`data/sql/base/db_world/creature.sql:148358-148370`): (2105.04, -292.56), (2092.95, -263.00),
  (2104.94, -233.44), (2124.30, -222.60), (2145.50, -222.62), (2164.20, -233.47), (2164.55, -293.00).

The orbs sit **13.5 yd above the arena floor**, and the radius test is 3D, so the 35 yd sphere cuts
the floor as a **32.3 yd circle** (`sqrt(35² − 13.5²)`) centred under the orb. That is why the trace
shows a knife-edge boundary: at 1:14 Malediction at (2125.1, −264.4) was 34.9 yd from the orb at
(2092.95, −263) and took every tick, while Ecoterrorist 3 yd further east was at 37.6 yd and took
none.

Cost:

| trace | Lightning Shock damage | share of all damage taken | hits |
|---|---|---|---|
| 1787597345 | 503 097 | **50.3%** | 340 |
| 1787600518 | 108 776 | 4.9% | 69 |

Nothing in the module reads aura 62016. Bots stand in the field for its full 15 s — Prayer took ticks
continuously from 1:23 to 1:54 without moving.

### Mechanic 2 — Deafening Thunder, the whole squad in one blast

- `EVENT_THORIM_STORMHAMMER` casts **Stormhammer 62042** every 16 s at one random enemy within 100 yd
  (2451-2551 damage plus a 2 s stun, `MECHANIC_STUN`).
- `data/sql/base/db_world/spell_linked_spell.sql:562` — `(62042, 62470, 1, 'Thorim - Stormhammer')`,
  type `SPELL_LINK_HIT`. AzerothCore's hit handler makes the **hit unit** cast the linked spell on
  itself with Thorim as original caster, so the blast is centred on the player the hammer landed on
  (which is why the trace attributes it to Thorim but every group of applications clusters in the
  arena, not on the balcony).
- **Deafening Thunder 62470**: `SCHOOL_DAMAGE` 4625-5376 nature at `EffectRadiusIndex 14` = **8 yd**,
  plus `SPELL_AURA_HASTE_SPELLS` at **−75** at `EffectRadiusIndex 18` = **15 yd**, duration
  **8 000 ms**. −75% spell haste means casts take four times as long. Not dispellable.

There is no way to dodge it reactively: the target set is chosen inside `Spell::SelectSpellTargets`
and is not readable from a bot, and the aura is already applied by the time anything can see it. The
only lever is **how many bots one 15 yd blast covers**, and today that is nearly all of them:

```
  1:00.130 n=14  ring radius min/med/max 7.0/10.0/14.0  closest pair 1.2  worst 15yd cluster 11
  2:02.802 n=14  ring radius min/med/max 4.0/ 9.8/14.0  closest pair 0.0  worst 15yd cluster 13
```

Every sample from 0:49 on has 11-14 of the 14 arena bots inside a single 15 yd circle. Measured
exposure: 30 episodes / 228 s over 13 bots in 1787600518; 68 episodes / 542 s over 25 bots in
1787597345. Individual arena casters spent 18-32 s each at quarter cast speed.

The same clustering feeds **Dark Rune Champion Whirlwind 15578** (8 yd radius, 390 906 damage =
**17.6%** of everything taken in 1787600518, across 143 hits and 13 victims) straight into the ranged
and healers, because the ring at 10/14 yd is inside Whirlwind range of an add standing on the tank.

### Explicitly not in this plan

**The arena had no bot tank, and that is the largest single cause of the collapse.** Recorded here so
it is not rediscovered:

`AssignThorimSquads` (`src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp:160`) counts tanks with
`PlayerbotAI::IsTank(member)` over the whole roster. `IsTank` with `bySpec = false` returns
`botAi->ContainsStrategy(STRATEGY_TYPE_TANK)` for a bot, but for a **human** there is no bot AI so it
falls through to the spec check. In 1787600518 the human Dragon is a protection paladin (Ardent
Defender 66233, Hammer of the Righteous 53595 in the trace), so `IsTank(Dragon)` is true,
`GetMainTankGuid` returns Dragon, `IsMainTank(Bulwark)` is false, `tankCount` reads 2, and the
`tankCount > 1` guard added last round passes — so `take(gauntletTank)` sent Bulwark, the raid's only
*bot* tank, down the corridor. `thorim.squad = 2` for Bulwark in the trace confirms it, and Bulwark
died with the gauntlet group at (2153, −446).

Consequence, measured from the snapshot `target` column across 5 589 add-samples: Dragon held 36.0%
of arena melee-add attention and the rest went to whoever was nearest — Tree (healer) 11.3%,
Nightwarrior 7.2%, Fel 6.9%, Hellflame 3.6%. Both arena healers died first (Tree 2:22, Prayer 2:32)
and the squad was gone 18 s later.

The fix would be to count and pick tanks **bot-only**, the same way every other pick in that function
already does (`IsBotPlayer(member) && PlayerbotAI::IsTank(member)`), plus skipping tanks in the DPS
loop since `IsDps` can also be true for a protection paladin.

---

## Fix 1 — Dodge the charged Thunder Orb

**Files:** `src/Ai/Raid/Uld/Util/UldBossHelper.h`, `src/Ai/Raid/Uld/Util/UldEncounter_Thorim.{h,cpp}`,
`src/Ai/Raid/Uld/Trigger/UldTriggers_Thorim.{h,cpp}`, `src/Ai/Raid/Uld/Action/UldActions_Thorim.{h,cpp}`,
`src/Ai/Raid/Uld/UldTriggerContext.h`, `src/Ai/Raid/Uld/UldActionContext.h`,
`src/Ai/Raid/Uld/UldStrategy.cpp`, `src/Ai/Raid/Uld/UldMultipliers.cpp`

### 1a. Constants

`NPC_THORIM_THUNDER_ORB = 33378` already exists in `UldBossHelper.h:141`. Add next to
`SPELL_THORIM_LIGHTNING_ORB_VISUAL` (`:153`):

```cpp
// Phase 1's orb marker, the counterpart to the visual above. Charge Orb sits on one Thunder Orb for
// 15s and triggers Lightning Shock (62017) once a second: ~3k nature at 35 yd, measured in 3D.
constexpr uint32 SPELL_THORIM_CHARGE_ORB = 62016;
```

and in the Thorim constant block near `ULDUAR_THORIM_ARENA_RING_*`:

```cpp
// The orbs hang 13.5 yd above the floor and the radius check is 3D, so a 35 yd sphere cuts the floor
// as a 32.3 yd circle - which is why a bot three yards from a victim never took a tick. The margin
// covers the bot still walking when the next tick lands.
constexpr float ULDUAR_THORIM_CHARGED_ORB_RADIUS = 32.3f;
constexpr float ULDUAR_THORIM_CHARGED_ORB_MARGIN = 4.0f;

// Fan the dodgers out rather than stacking them on one antipodal point. Every orb is 42 yd from the
// centre, so +-30 degrees off the antipode is still 58 yd from the orb at the outer ring radius.
constexpr float ULDUAR_THORIM_ORB_DODGE_FAN_STEP = 0.2618f;  // 15 degrees
constexpr uint8 ULDUAR_THORIM_ORB_DODGE_FAN_SLOTS = 5;
```

### 1b. Reuse the existing orb scan

`ThorimChargedThunderOrb` (`UldEncounter_Thorim.cpp:1269`) already does exactly the right sweep —
`GetCreatureListWithEntryInGrid` for `NPC_THORIM_THUNDER_ORB`, cached once per instance per
`ULDUAR_THORIM_ENCOUNTER_SCAN_INTERVAL_MS` (500 ms) — but hardcodes
`SPELL_THORIM_LIGHTNING_ORB_VISUAL`. Parameterise it: `ThorimChargedThunderOrb(PlayerbotAI*, uint32 markerSpell)`,
and store the marker alongside the cache (`chargedOrbGuid`, new `orbScanSpell`) so a request for a
different marker rescans instead of returning a stale hit. Phase 1 (62016) and phase 2 (62186) never
overlap, so the cache never thrashes. Update the one existing caller in `ThorimLightningChargeActive`
to pass `SPELL_THORIM_LIGHTNING_ORB_VISUAL`. Do **not** add a second 150 yd grid sweep.

### 1c. Geometry helper

In `UldEncounter_Thorim.cpp`, declared in the header next to `GetThorimArenaAnchor`:

```cpp
bool ThorimChargedOrbEscape(PlayerbotAI* botAI, Player* bot, Position& out);
```

- Return false unless the bot is arena squad and Thorim is still above
  `ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD` (phase 1 only — phase 2 has its own ring and its own orb).
- Find the orb via `ThorimChargedThunderOrb(botAI, SPELL_THORIM_CHARGE_ORB)`; no orb → false.
- Return false when the bot's **2D** distance to the orb already exceeds
  `ULDUAR_THORIM_CHARGED_ORB_RADIUS + ULDUAR_THORIM_CHARGED_ORB_MARGIN` (36.3 yd).
- Otherwise build the escape point: bearing from the orb through
  `ULDUAR_THORIM_NEAR_ARENA_CENTER`, fanned by the bot's ring slot —
  `angle = antipode + ULDUAR_THORIM_ORB_DODGE_FAN_STEP * (slot % ULDUAR_THORIM_ORB_DODGE_FAN_SLOTS - 2)`.
  Radius = the bot's own anchor radius: `ULDUAR_THORIM_ARENA_RING_OUTER` for an outer-ring bot,
  `ULDUAR_THORIM_ARENA_RING_INNER` for everyone else (inner ring, melee, tank).
- Finish exactly as `GetThorimArenaRingSlot` does: `GetMapWaterOrGroundLevel`, then
  `CheckCollisionAndGetValidCoords`, then the existing clamp — if the result lands further than
  `ULDUAR_THORIM_ARENA_LEASH_RADIUS` from the centre, hand back the centre instead. The centre is
  42 yd from every orb, so it is always a safe answer.

The slot index is the one `GetThorimArenaRingSlot` already computes from the latched arena squad in
group order; factor that ordering out into a small shared helper rather than duplicating the sort, so
the fan and the ring cannot disagree about who is slot 3.

### 1d. Trigger and action

- `ThorimChargedOrbTrigger : Trigger` (`"thorim charged orb trigger"`) — `IsActive()` is
  `Position spot; return ThorimChargedOrbEscape(botAI, bot, spot);`
- `ThorimChargedOrbAction : MovementAction` (`"thorim charged orb action"`) — recompute the escape
  point and `MoveTo` it with `MovementPriority::MOVEMENT_COMBAT`. Follow the reach-then-hold shape
  documented in `docs/engine/pitfalls.md`: accept arrival on
  `ULDUAR_THORIM_RING_ARRIVE_TOLERANCE`, return `true` while still moving, yield once stopped.
- Register in `UldTriggerContext.h` and `UldActionContext.h`. **Both**, or the node is skipped with no
  warning.
- Wire in `UldStrategy.cpp`, in the Thorim block, at **`ACTION_RAID + 4`** — above the two Sif dodges
  and the corridor smash (+3), below the arena leash (+5), which must always win because a bot
  outside the box summons the Lightning Orb. `thorim lightning charge action` also sits at +4 but is
  phase-2 gated, so the two can never be live together.

### 1e. Stop the anchor fighting the dodge

- `ThorimArenaPositioningTrigger::IsActive()` (`UldTriggers_Thorim.cpp:101`) already backs off while
  the Sif dodges are active. Add the same early `return false` for `ThorimChargedOrbTrigger`.
- Add `"thorim charged orb action"` to the `encounterMovers` sets in
  `ThorimArenaLeashMultiplier::GetValue` (`UldMultipliers.cpp:360`) and
  `ThorimArenaAnchorGuardMultiplier::GetValue` (`:402`). Without the second one, a bot that has
  latched `arenaAnchorArrived` has every mover zeroed and cannot step out of the field.

### 1f. Observability

Note the dodge so the next trace can be read without inferring it: emit `thorim.orbescape` (the
chosen destination, same string form as the existing `thorim.anchor` notes) when the action first
issues a move, and clear the state in `ResetThorimEncounterState` alongside `chargedOrbGuid` /
`orbScanMs` (`UldEncounter_Thorim.cpp:1380`). Check the schema in `docs/systems/observability.md`
before adding the key.

## Fix 2 — Open the arena ring to 13 / 18

**File:** `src/Ai/Raid/Uld/Util/UldBossHelper.h`

```cpp
constexpr float ULDUAR_THORIM_ARENA_RING_INNER = 13.0f;   // was 10
constexpr float ULDUAR_THORIM_ARENA_RING_OUTER = 18.0f;   // was 14
```

Update the comment above them to say why: 10/14 put every ranged bot inside a Champion's 8 yd
Whirlwind and inside one 15 yd Deafening Thunder blast with the entire rest of the squad. 13/18 is
the largest opening that still respects both hard limits — arena adds `MoveJump` to **19-24 yd** from
the centre (`boss_thorim.cpp` `SpawnAnArenaNPC`), so anything at or past 19 puts ranged in the drop
zone, and the navmesh gives out past ~26 yd on the south side.

Effect: 5 inner slots go from 11.8 to 15.3 yd apart, 5 outer from 16.5 to 21.2, and the worst 15 yd
cluster drops from 11-14 bots to roughly 6. It does not eliminate Deafening Thunder — nothing can, see
Context — it stops one hammer taking the whole squad's cast speed at once.

No new constant is needed for the melee: `ULDUAR_THORIM_ARENA_MELEE_LEASH` (24) already covers the
widened ring.

## Verification

Already done, do not repeat:

- navprobe, map 603, rings around the arena centre (2134.9854, −263.11853, 419.8465):
  **13 yd 12/12 on mesh**, **18 yd 12/12**, 14 / 20 / 24 yd 16/16. Radius 28 fails — the 270° point
  (2134.985, −291.119) settles to z −27.7. Command:
  `MSYS_NO_PATHCONV=1 docker run --rm -v azerothcore-wotlk-pb_ac-client-data:/azerothcore/env/dist/data:ro --entrypoint /azerothcore/env/dist/bin/navprobe acore/ac-wotlk-build:master --map 603 ring 2134.9854 -263.11853 419.8465 <R> 16`

Static, before any pull:

- `grep -n "thorim charged orb" src/Ai/Raid/Uld/UldTriggerContext.h src/Ai/Raid/Uld/UldActionContext.h src/Ai/Raid/Uld/UldStrategy.cpp` — three hits for the trigger, three for the action. An unregistered name is skipped silently.
- `grep -n "encounterMovers" -A 6 src/Ai/Raid/Uld/UldMultipliers.cpp` — the new action is in the leash and anchor-guard sets.
- `grep -rn "ThorimChargedThunderOrb" src/Ai/Raid/Uld` — every call site passes a marker spell.
- The module cannot be compiled headless in this environment; hand the branch off for a build rather
  than claiming one.

In-game, one normal-mode 25-man pull, then re-run the analysis on the fresh trace:

1. **Lightning Shock damage collapses.** In 1787597345 it was 503 097 (50.3% of everything taken) over
   340 hits; in 1787600518, 108 776 over 69. Target: under ~40 000 total, and no bot taking more than
   3 consecutive ticks — that is the reaction window, not a failure.
2. **No bot stands in the field.** For every `dmg` record with `sp` 62017, the victim's 2D distance to
   the charged orb should be inside 32.3 yd only in the first second or two of the window.
3. **`thorim charged orb action` moves stay in bounds.** Every destination within 30 yd of
   (2134.99, −263.12) and inside the arena box; none refused with `nopath` or `blocked`.
4. **The ring is wider.** Ring slots land at 13 and 18 yd, and the "worst 15 yd cluster" figure from
   the spread check drops from 11-14 to ~6.
5. **Deafening Thunder exposure falls.** Baseline 30 episodes / 228 s over 13 bots (1787600518).
   Expect roughly half the episodes for the same number of hammers.
6. **Whirlwind damage falls.** Baseline 390 906 across 13 victims; the ranged should largely drop out
   of the victim list.
7. **No new freeze.** Every living arena bot still holds a target in well over half its samples and
   keeps moving; no `thorim arena anchor guard killed thorim charged orb action` vetoes.

The `thorim2.py` / `shock.py` / `orbs.py` / `aggro.py` helpers used for this investigation are in the
session scratchpad and do not need checking in — they are a few minutes to re-derive from
`postmortem.py`'s `Trace` class.

## Follow-up, not in this plan

- The bot-tank count bug above. It is the biggest single cause of the arena collapse and is
  deliberately excluded from this round.
- Write both mechanics into `docs/raids/ulduar.md` (Thorim section), including the Charge Orb
  geometry and the seven orb spawns, so the 3D-radius trap is not re-derived. Still carrying the
  earlier round's unwritten findings too (burst-cooldown suppression, `DpsAoeStrategy` dead wiring,
  `prioritized targets` never clearing inside a combat).
- 1787597345 ran with no squad split at all — the whole raid sat in the arena for three minutes and
  no `thorim.squad` note was ever written, so `AssignThorimSquads` returned early for the entire pull.
  Worth a separate look; the most likely cause is the `GetMainTankGuid().IsEmpty()` gate never
  clearing.
