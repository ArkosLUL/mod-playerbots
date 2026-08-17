# Freya (Ulduar) — Nature Bomb dodging and burst-cooldown gating

## Context

Follow-up to the shipped add-wave work (`82c45a8f9`). Two new in-game failures:

1. **Melee bots die to Nature Bombs.** They do try to dodge — the node is wired correctly — but the
   movement primitive cannot clear the blast, so every bombed bot eats the full hit.
2. **DPS burst cooldowns fire on the pull**, into a boss who cannot be killed for another ~6 minutes.
   Heroism is correctly held; nothing else is.

Intended outcome: a Nature Bomb volley costs the raid no deaths, and no cooldown is spent on Freya
while she cannot be killed.

## Ground truth (verified — do not re-derive)

### Nature Bomb

| Fact | Value | Source |
|---|---|---|
| Damage 64587 | 5850–6150 nature, **EffectRadiusIndex 13 = 10 yd** | `mod-spell-tweaks/data/dbc-reference/spell.reference.csv` |
| Difficulty scaling | **none** — `SpellDifficultyID = 0`, no `spelldifficulty` row. 10 m == 25 m | same |
| Landing spot | the target player's **own feet** — `SpellHitTarget(SPELL_NATURE_BOMB_FLIGHT)` summons NPC 34129 at `target->GetPosition()` | `boss_freya.cpp:596-597` |
| Fuse | **~6 s**: `_explodeTimer` hits 5000 → jumps to 10000 + GO goes `GO_STATE_ACTIVE`; next tick reaches 11000 → detonate | `boss_freya.cpp:1300-1317` |
| Volley | 7-10 bombs in 25 m (3-4 in 10 m), **one per player** within 70 yd, repeating every **18 s** | `boss_freya.cpp:645-660` |
| Marker | GO **194902** (`gameobject_template` type 10, confirmed present in `acore_world`), summoned in the bomb NPC's `Reset()` and alive for the whole fuse | `boss_freya.cpp:1295` |

Volleys are 18 s apart and the fuse is 6 s, so **all live bombs are always from the same volley** —
walking through one on the way out is harmless. No path-safety check is needed.

### Why the current dodge cannot work

[UldActions_Freya.cpp:51](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Freya.cpp#L51)
calls `FleePosition(bombPos, 13.0f)`. Inside
[MovementActions.cpp:2152](modules/mod-playerbots/src/Ai/Base/Actions/MovementActions.cpp#L2152) and
its ranged twin at `:2215`:

```cpp
float fleeDis = std::min(radius + 1.0f, sPlayerbotAIConfig.fleeDistance);
```

`AiPlayerbot.FleeDistance = 5.0` (conf; **confirmed no `AC_*` env override** on the running
`ac-worldserver`). The 13 yd argument is discarded — the bot steps **5 yd out of a 10 yd blast from
dead centre**. Arithmetically impossible to survive.

Two compounding defects in the same primitive:

- [`BestPositionForMeleeToFlee`:2119-2135](modules/mod-playerbots/src/Ai/Base/Actions/MovementActions.cpp#L2119-L2135)
  — with a current target the candidate angles are perpendicular-to-target and straight *at* the
  target. The away-from-the-hazard angle is added **only when `isTanking`**, so non-tank melee never
  even consider running away from the bomb.
- Only the *nearest* bomb is read
  (`FindNearestGameObject(GOBJECT_NATURE_BOMB, 12.0f)`); with melee stacked on a spore, several bombs
  land in the pile and the destination is not tested against the others. `CheckLastFlee` then
  blacklists any angle within 45° of a recent flee's reverse for 5 s, so the second bomb of a volley
  often yields `Position()` → `FleePosition` returns `false` → the action returns `false` → the bot
  falls through to attacking and stands in the blast.

`FleePosition` is therefore unusable for any hazard wider than ~5 yd. This generalises well beyond
Freya and belongs in the engine docs.

### Burst window

[UldMultipliers.cpp:489-492](modules/mod-playerbots/src/Ai/Raid/Uld/UldMultipliers.cpp#L489-L492)
returns `{allowAll = true, allowLust = <Attuned gone>}` for Freya, so everything in
`burstCooldownNames`
([BurstCooldowns.cpp:22-46](modules/mod-playerbots/src/Ai/Base/Combat/BurstCooldowns.cpp#L22-L46) —
Recklessness, Icy Veins, Combustion, Avenging Wrath, trinkets, potions, …) is ungated from the pull.

- Attuned to Nature **62519** is not damage reduction: it is `SPELL_AURA_MOD_HEALING_PCT`, **+8 % per
  stack × 150 stacks = +1200 % healing received**, applied in `JustEngagedWith`. Combined with
  Lifebinder's Gift every 45 s, damage on Freya before the final phase is wasted. The
  `SPELL_ATTUNED_TO_NATURE` comment at `UldBossHelper.h:93` says "damage reduction" and is wrong.
- Adds do not spawn until **10 s after engage** (`EVENT_FREYA_ADDS_SPAM` scheduled at 10 s), so the
  opener has nothing but Freya to spend on.
- Stacks come off only when wave adds die: Conservator 25, each trio member 10, each Detonating
  Lasher 2 (`boss_freya.cpp:1133-1148`). Six waves × (25 + 30 + 20) = exactly 150, and the seventh
  `EVENT_FREYA_ADDS_SPAM` tick strips the aura regardless. **The adds phase is time-capped, not
  damage-capped** — so cooldowns spent on adds are not stolen from the final burn, and 2-3 min
  cooldowns are back up by then either way.

## Design decisions (settled with the user — do not relitigate)

- **Ring sampler, not `FleePosition`.** Sample rings outward from the bot; first spot clear of *every*
  live bomb wins; `MoveTo` with `MOVEMENT_FORCED`. Bypasses the conf cap entirely.
- **Tanks eat Nature Bombs.** Both of them. The main tank never drags Freya and the add tank never
  leaves the parked spore, at the cost of ~6 k per volley.
- **Burst held until the final phase**, same condition as lust. "Allowed on wave adds" was chosen
  first and then withdrawn: the adds are not boss-flagged, so `HoldBurstUntilTankEngagedMultiplier`
  zeroes burst on them regardless of what `allowAll` says, and a `0.0f` veto is final.
- Lust/heroism gating is already correct and stays as it is.

## Patterns being reused

- `SupremusMoveAwayFromVolcanosAction::FindSafestNearbyPosition`
  ([BTActions.cpp:374-432](modules/mod-playerbots/src/Ai/Raid/BT/BTActions.cpp#L374-L432)) — the ring
  sampler this is modelled on. Generalised into a shared helper here; the two BT copies are left
  alone (out of scope).
- `FreyaWaveState` / `GatherFreyaWaveState` / `FreyaTrioSyncSuppress`
  ([UldBossHelper.h:757-782](modules/mod-playerbots/src/Ai/Raid/Uld/Util/UldBossHelper.h#L757-L782)) —
  reused by the burst gate.

---

## 1. Shared helper — `Ai/Raid/RaidBossHelpers.{h,cpp}`

```cpp
// Nearest spot at least clearRadius from every hazard, sampled on rings outward from the bot.
// Returns Position() when nothing inside maxRadius is clear.
Position FindNearestPositionClearOfHazards(Player* bot, std::vector<Position> const& hazards,
                                           float clearRadius, float maxRadius,
                                           float distanceStep = 2.0f,
                                           float angleStep = float(M_PI) / 8.0f);
```

Same ring/angle sweep as the Supremus version, with three differences: it takes `Position`s (Nature
Bombs are GameObjects, not Units), it returns the *first* clear ring rather than a best-effort
fallback, and it drops the path-safety pass — the hazards it is built for share one fuse, so the walk
out is safe. Reject candidates that fail
`bot->GetMap()->CheckCollisionAndGetValidCoords(...)` before accepting them.

## 2. Bomb positions — `UldBossHelper.{h,cpp}`

```cpp
// GameObjects, not creatures: the bomb NPC is banished and never appears in the npc value lists.
std::vector<Position> GetFreyaNatureBombPositions(Player* bot, float searchRadius);
```

`bot->GetGameObjectListWithEntryInGrid(list, GOBJECT_NATURE_BOMB, searchRadius)`, collecting
positions.

New constants beside `ULDUAR_FREYA_DETONATE_RADIUS`:

```cpp
// 64587 is 10 yd flat in both raid sizes; the extra yard covers the bot's own reach so it does not
// clip the edge of the blast standing still.
constexpr float ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS = 11.0f;
// Aim past the trigger radius. Landing exactly on the boundary would re-fire the node every tick as
// combat movement pulls the bot back toward its target.
constexpr float ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS = 13.0f;
constexpr float ULDUAR_FREYA_NATURE_BOMB_SEARCH_RADIUS = 30.0f;
```

Also fix the wrong `SPELL_ATTUNED_TO_NATURE` comment at `UldBossHelper.h:93`.

## 3. Trigger — `UldTriggers_Freya.cpp`

`FreyaNearNatureBombTrigger::IsActive`:

- Freya alive;
- **`botAI->IsTank(bot)` → false** — tanks eat it (mirrors the exclusion in
  `FreyaAvoidDetonatingLasherTrigger`, with its own reason: Freya is never dragged and the Conservator
  is never unparked);
- any live `GOBJECT_NATURE_BOMB` within `ULDUAR_FREYA_NATURE_BOMB_AVOID_RADIUS`.

## 4. Action — `UldActions_Freya.cpp`

`FreyaMoveAwayNatureBombAction::Execute` drops `FleePosition` entirely:

```
bombs = GetFreyaNatureBombPositions(bot, ULDUAR_FREYA_NATURE_BOMB_SEARCH_RADIUS)
if bombs empty                                      -> false
safe = FindNearestPositionClearOfHazards(bot, bombs,
           ULDUAR_FREYA_NATURE_BOMB_CLEAR_RADIUS,
           ULDUAR_FREYA_NATURE_BOMB_SEARCH_RADIUS)
if safe == Position()                               -> false
MoveTo(safe, ..., MovementPriority::MOVEMENT_FORCED, true, false)
```

`isUseful()` delegates to the trigger, as the other Freya actions do.

Priority is unchanged (`ACTION_RAID + 4`) and already outranks the spore node, so a bomb landing on
the melee stack scatters them and the spore node re-converges them afterwards — the churn the shipped
plan predicted and deliberately left untuned.

## 5. Burst gate — `UldMultipliers.cpp`

Replace the Freya branch of `UlduarBurstWindowMultiplier::EvaluateWindow`:

```cpp
if (freya)
{
    bool const finalPhase = !freya->HasAura(SPELL_ATTUNED_TO_NATURE) ||
                            freya->GetHealthPct() <= FREYA_LUST_FALLBACK_PCT;

    return {finalPhase, finalPhase};
}
```

Verified constraint behind the collapse: `HoldBurstUntilTankEngagedMultiplier`
([BurstWindowStrategy.cpp:62-72](modules/mod-playerbots/src/Ai/Base/Strategy/BurstWindowStrategy.cpp#L62-L72))
returns `0.0f` for any burst cooldown whose current target fails `IsDungeonBoss() || isWorldBoss()`,
under `AiPlayerbot.BurstOnBossOnly` (conf `1`, default `true`, **no `AC_*` override**). Every Freya
wave add is `flags_extra = 0`, `rank = 1` in `creature_template`, so a per-boss `allowAll` of `1.0f`
cannot reach them - multipliers multiply.

## 6. Docs

- `docs/raids/ulduar.md`, Freya section: the Nature Bomb table above, the tanks-eat-it rule, and the
  burst gate rule.
- `docs/engine/pitfalls.md`: **`FleePosition` silently clamps its travel to
  `AiPlayerbot.FleeDistance` (5.0) and discards the `radius` argument, so it cannot clear any hazard
  wider than ~5 yd; and for non-tank melee its candidate angles never include "away from the
  hazard".** Point at the ring-sampler helper as the answer for wide hazards.

`compact-governing-docs` applies: **invoke `/compact-docs-writer` before touching these two**, as a
fresh invocation — the one used earlier in this session covered a closed authoring cycle.

No CMake change: `RaidBossHelpers.cpp` and every Freya file already exist. No new trigger/action
names, so no context or strategy registration.

## Deliberately out of scope

- Fixing `FleePosition` / `BestPositionFor*ToFlee` for the rest of the codebase, or raising
  `AiPlayerbot.FleeDistance` globally.
- Deduplicating the two existing BT ring-sampler copies onto the new helper.
- Re-parking the Conservator away from a bombed spore (still deferred from the shipped plan).

## Added during implementation (beyond the approved plan)

`FreyaAvoidDetonatingLasherAction` and `FreyaDodgeUnstableSunBeamAction` both used `FleePosition`
against a 15 yd and a 12 yd hazard, so both were silent no-ops for the same reason as the bomb dodge.
Both moved onto `FindNearestPositionClearOfHazards`, clearing every lasher / every beam in range.

## Verification

The module cannot be compiled headless here, so the build is a hand-off.

1. Static checks: `FleePosition` no longer appears in `UldActions_Freya.cpp`; the new helper is
   declared and defined once.
2. Build the worldserver with the module.
3. **Nature Bomb volley, 25 m.** On the 18 s cadence, non-tank bots visibly scatter and take **zero**
   bomb damage. Failure signature is unchanged: bots stepping a few yards and still taking ~6 k.
4. Watch a volley land on the melee stack during a Conservator wave — bots should scatter clear of
   *all* bombs at once, not walk from one into another, then re-converge on the parked spore.
5. Confirm both tanks stay put and take the hit, and that Freya and the Conservator do not move.
6. **Pull.** No burst cooldown fires in the first 10 s, and none during the wave phase at all.
7. Confirm every burst cooldown, lust included, opens when Attuned to Nature drops.
8. **Detonate and (hard mode) Unstable Sun Beam**: a low-health bot steps fully clear of the blast
   instead of a few yards, and clear of the whole cluster where several overlap.
9. Confirm `Attuned to Nature` still reaches 0 so the final phase starts, and that wave clear times
   have not regressed.
10. Repeat in 10 m and with `UlduarFreyaHardMode = true`.

On approval this plan is saved to `docs/plans/freya-nature-bomb-burst/freya-nature-bomb-burst.PLAN.md`.
Two shipped plan dirs (`docs/plans/freya-add-waves`, `docs/plans/freya-trio-sync`) are still present
against the delete-on-ship rule in `docs/README.md`; removing them is a separate call.
