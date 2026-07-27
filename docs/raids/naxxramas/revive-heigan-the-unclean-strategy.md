# Revive Heigan the Unclean — gap analysis + rebuild

## Context

`RaidNaxxStrategy` has Heigan the Unclean fully disabled: commit `f85a25760` ("Commented out
Heigan strategy") comment-out 455 lines across 10 files without deleting anything. Bots in
Naxxramas currently have zero Heigan behaviour — no dance, no dispel, no movement suppression.

Simply un-commenting is **not safe**. Auditing the disabled code against the current core script
(`src/server/scripts/Northrend/Naxxramas/boss_heigan.cpp`, `instance_naxxramas.cpp`) shows the
Phase 1 dance never advances at all, and the pull is mis-detected as Phase 2. Both are guaranteed
raid wipes. This plan revives the encounter behind a new `HeiganBossHelper` with a corrected
phase/tick model, plus 25-man support, ranged ledge handling, threat redirect, and melee uptime.

---

## Gap analysis (verified against the core)

### G1 — Phase 1 dance never advances (fatal)

`HeiganDanceAction::CalculateSafe` advances the safe zone by watching the boss cast Eruption:

```cpp
Spell* spell = boss->GetCurrentSpell(CURRENT_GENERIC_SPELL);
bool isEruption = NaxxSpellIds::MatchesAnySpellId(info, {NaxxSpellIds::Eruption10});  // 29371
```

The boss never casts 29371. `instance_naxxramas.cpp:254-267` erupts via the floor GameObjects:

```cpp
for (GameObject* go : _heiganEruption[i])
    go->CastSpell(nullptr, SPELL_ERUPTION);
```

`boss_heigan` only casts Spell Disruption (29310), Decrepit Fever (29998), Plague Cloud (29350) and
Teleport Self (30211). So in Phase 1 `NextSafe()` is never called: every bot parks on
`waypoints[0]` (core section 3) for the whole 90 s slow dance. Section 3 is safe on only ~2 of the
8 Phase-1 ticks — bots eat roughly 6 eruptions per Phase 1.

The old fallback (`EqualLowercaseName(..., "eruption")`) does not help; it reads the same
never-populated boss cast slot. The `eruption_casting` branch in `HeiganDanceMultiplier::GetValue`
is dead for the same reason, so the Phase-1 movement lockdown never engages either.

### G2 — The pull is mis-detected as Phase 2 (fatal)

Phase detection is positional:

```cpp
platform_phase = boss->IsWithinDist2d(platform.first /*2794.26*/, platform.second /*-3706.67*/, 10.0f);
```

Heigan's DB spawn (`data/sql/base/db_world/creature.sql:121519`) is
`map 533, 2793.86, -3707.38, 276.627, o 2.4` — **on the platform**, ~0.8 yd from the check point,
at `platformZ`, facing `heiganFastDanceFaceDirection`. At the pull the bot therefore believes it is
in the fast dance: it seeds `phase2_start_ms` from the pull, dances on the 4 s Phase-2 cadence
while the real fight runs Phase-1 15 s/10 s ticks, and the multiplier zeroes all combat actions so
nobody attacks.

The same false positive recurs after every Phase 2: `StartFightPhase(PHASE_SLOW_DANCE)` does **not**
teleport the boss down — it just flips to `REACT_AGGRESSIVE` and lets him walk off the ledge. The
positional check stays true for several seconds into Phase 1.

### G3 — No reset on the Phase 2 → Phase 1 edge

`StartFightPhase` sets `_currentSection = 3` at the start of **every** phase
(`boss_heigan.cpp:99`). The disabled code only calls `ResetSafe()` when entering the platform
phase, never when leaving it, so Phase 1 restarts from a stale index. Moot today because of G1,
live once G1 is fixed.

### G4 — Decrepit Fever dispel is dead on 25-man

`HeiganDispelDecrepitFeverAction` matches `NaxxSpellIds::DecrepitFever` = 29998 only.
`data/sql/base/db_world/spelldifficulty_dbc.sql:99` maps `(29998, 29998, 55011, 0, 0)`, so 25-man
players carry **55011**. `HasAura(29998)` never matches and no bot ever dispels. Eruption (29371),
Spell Disruption (29310) and Plague Cloud (29350) have no difficulty rows — single ID is correct
for those.

### G5 — Melee never attack in Phase 1

```cpp
return MoveInside(bot->GetMapId(), waypoints[curr_safe].first, ..., arenaZ,
                  botAI->IsMainTank(bot) ? 0 : 0, MovementPriority::MOVEMENT_COMBAT);
```

`MovementActions.cpp:1692` returns false only when `GetDistance2d(x,y) <= distance`; with
`distance == 0` that is effectively never, so the action succeeds every tick, out-prioritises
combat, and pins every non-tank melee on the exact waypoint centre doing zero damage. `MoveNear`
then offsets by `cos(angle)*distance` — at distance 0 all 25 bots stack on one point. The
`IsMainTank(bot) ? 0 : 0` ternary is dead code that was clearly meant to differentiate them.

### G6 — Ranged leave the ledge too late

`HeiganDanceRangedAction` parks ranged on the platform during Phase 1 (correct — the ledge is the
standard ranged spot) but only moves them off once `platform_phase` is already true, i.e. after
Heigan has teleported up. They then have 7 s to path down a ramp while Plague Cloud (29350) lands
on the ledge 1 s after the teleport (`boss_heigan.cpp:137-139`).

### G7 — No threat redirect case

`docs/raids/naxxramas/naxx-threat-redirect-findings.md:106` already flags Heigan (Tier 3): the
Phase 2 → Phase 1 return is a clean re-pull that wants a redirect, blocked only on the strategy
being disabled.

### Not broken — verified good, keep as-is

- **Waypoint geometry.** Feeding each waypoint through the core's `GetEruptionSection` classifier
  (origin `(2796, -3707)`, slopes `-0.3056 / -1.2766 / -2.8000`) confirms the mapping
  `module index i ↔ core section 3 - i`, and each point sits comfortably inside its wedge:

  | idx | waypoint | core section | nearest *erupting* GO |
  |---|---|---|---|
  | 0 | `2794.88, -3668.12` | 3 | 13.63 yd |
  | 1 | `2775.49, -3674.43` | 2 | 13.60 yd |
  | 2 | `2762.30, -3684.59` | 1 | 13.65 yd |
  | 3 | `2755.99, -3703.96` | 0 | 11.36 yd |

  Keep these coordinates. Note the margins when picking the `MoveInside` spread radius in G5 —
  index 3 is the tight one.

- **`NextSafe()` / `ResetSafe()` index walk.** Produces `0,1,2,3,2,1,0,1,2,3,…`, which is exactly
  the core's `3,2,1,0,1,2,3,…` under the reversed mapping. The `uint32 curr_dir = -curr_dir`
  looks wrong but is correct by unsigned wraparound (`+= 0xFFFFFFFF` ≡ `-= 1`) and the index never
  leaves `[0,3]`. Switch to `int32` for clarity and drop the tautological
  `assert(curr_safe >= 0)` (a `-Wtype-limits` warning on unsigned), but the logic itself stands.

---

## Design

### Phase and tick model

The core eruption schedule carries **no RNG** — only Spell Disruption and Decrepit Fever are
randomised. Both phases are exactly determined:

| Phase | First eruption | Period | Ticks | Phase length | Safe index sequence |
|---|---|---|---|---|---|
| 1 (slow, arena) | +15 s | 10 s | 8 | 90 s | `0,1,2,3,2,1,0,1` |
| 2 (fast, ledge) | +7 s | 4 s | 10 | 45 s | `0,1,2,3,2,1,0,1,2,3` |

So bots need only a reliable **phase-start edge**; the index follows arithmetically and re-anchors
every 45–90 s, so drift can never accumulate.

Phase discriminator — replace the positional check entirely:

```
Phase 2  ⇔  boss->ToCreature()->GetReactState() == REACT_PASSIVE   (set at fast-dance start,
                                                                    cleared at slow-dance start)
         or boss->HasAura(29350)  // Plague Cloud, secondary confirmation
```

This is exact and has no lag, unlike the position check, and immediately kills G2. The existing
Gothik code (`NaxxActions_Gothik.cpp:363`) already uses the `ToCreature()->GetReactState()`
pattern, so it is an established idiom here.

Anchors, in order of preference:
1. **Combat start** → Phase 1 anchor (the `_combat_start_ms` pattern from `GluthBossHelper`).
2. **Any observed react-state edge** → re-anchor and `ResetSafe()`.
3. **No anchor yet** (bot rezzed or arrived mid-fight) → report "unknown"; the dance action must
   bail out rather than guess a zone. It self-heals at the next phase edge, ≤90 s.

### `HeiganBossHelper`

Follow `GluthBossHelper` (`NaxxBossHelper.h:1036`) exactly — plain `AiObject`, no `EventMap`.
`boss_heigan` is `TaskScheduler`-driven and exposes no `events` member, so the templated
`GenericBossHelper<BossAiType>` cannot be used. `instance_naxxramas` has no `GetData` override
either, so `DATA_HEIGAN_ERUPTION` is write-only — the timer model is the only module-side option.

Surface:

```cpp
class HeiganBossHelper : public AiObject
{
public:
    const std::pair<float, float> platform = {2794.26f, -3706.67f};
    const float platformZ = 276.54f;
    const float arenaZ    = 264.00f;
    const std::vector<std::pair<float, float>> waypoints = { /* the four above */ };

    HeiganBossHelper(PlayerbotAI* botAI) : AiObject(botAI) {}
    bool UpdateBossAI();          // resolve boss, detect phase edge, re-anchor, advance index
    bool IsFastDance() const;     // phase 2
    bool IsSynced() const;        // false until an anchor exists
    uint32 SafeIndex() const;     // 0..3, only valid when IsSynced()
    uint32 MsUntilNextEruption() const;
    bool JustReturnedToArena() const;   // phase 2 -> phase 1, for the threat redirect
    Unit* GetBoss() const;
private:
    void Reset();
    // _unit, _combat_start_ms, _phase_start_ms, _last_fast_dance, _last_ticks, curr_safe, curr_dir
};
```

Each trigger/action/multiplier owns its own `HeiganBossHelper helper;` member, matching the
Gluth/Loatheb/Sapphiron convention.

---

## Files to change

All under `modules/mod-playerbots/src/Ai/Raid/Naxx/`, plus one doc.

1. **`NaxxSpellIds.h:36-43`** — un-comment and extend:
   `Eruption = 29371`, `DecrepitFever10 = 29998`, `DecrepitFever25 = 55011`,
   `SpellDisruption = 29310`, `PlagueCloud = 29350`, `TeleportSelf = 30211`.
   Use the existing `NaxxSpellIds::HasAnyAura(unit, {DecrepitFever10, DecrepitFever25})` helper
   (`NaxxSpellIds.h:142`) for G4.

2. **`NaxxBossHelper.h`** — add `HeiganBossHelper` next to `GluthBossHelper` (~line 1036), per the
   surface above. This is where the phase/tick model lives; nothing else duplicates it.

3. **`Action/NaxxActions.h:89-162`** — un-comment the four classes; strip the now-dead
   `last_eruption_ms` / `platform_phase` / `phase2_*` / `curr_safe` / `curr_dir` / coordinate
   members from `HeiganDanceAction` and replace with a `HeiganBossHelper helper;`. `CalculateSafe()`
   goes away — the helper owns it.

4. **`Action/NaxxActions_Heigan.cpp`** — rewrite the bodies:
   - `HeiganDanceMeleeAction::Execute` — bail if `!helper.IsSynced()`. Main tank keeps the boss
     in the safe zone; other melee use a non-zero `MoveInside` radius so they settle and resume
     DPS (G5). Keep the radius under the clearances in the table above — index 3 only has
     11.36 yd, so ~5 yd is the ceiling.
   - `HeiganDanceRangedAction::Execute` — Phase 1 on the ledge as today, but leave **pre-emptively**
     when `helper.MsUntilNextPhase()` is inside a departure window (G6), and never re-enter while
     Plague Cloud is up.
   - `HeiganDispelDecrepitFeverAction` — swap the raw `HasAura(29998)` for the 10/25 helper (G4);
     keep the existing main-tank-first target selection and the Paladin/Priest/Shaman cast order.

5. **`NaxxTriggers.h:64-83` + `NaxxTriggers.cpp:95-158`** — un-comment; give each trigger a
   `HeiganBossHelper helper;` and drive `IsActive()` off `helper.UpdateBossAI()` instead of a raw
   `AI_VALUE2(Unit*, "find target", ...)`. Set an explicit short `checkInterval` (the
   `MutatingInjectionTrigger` precedent uses `1`) — the Phase-2 4 s cadence needs it.

6. **`NaxxMultipliers.h:22-29` + `NaxxMultipliers.cpp:50-118`** — un-comment `HeiganDanceMultiplier`;
   fix the `"helgan dance"` name typo; replace the dead `eruption_casting` branch with
   `helper.IsFastDance() || helper.MsUntilNextEruption() < <window>`. Keep the existing
   `CastAspectOfThePackAction` rule and its comment — the Phase-1 daze reasoning is still valid.

7. **`NaxxMultipliers.cpp` — `NaxxThreatRedirectMultiplier` (~:547)** — add the Heigan case (G7),
   gated on `helper.JustReturnedToArena()`, so redirects fire on the ledge→arena re-pull and are
   inert otherwise.

8. **`NaxxActionContext.h:24-26, 77-79`** and **`NaxxTriggerContext.h:23-25, 75-77`** —
   un-comment the three creators and three factories in each.

9. **`NaxxStrategy.cpp:30-42, 217`** — un-comment the three `TriggerNode`s and the
   `HeiganDanceMultiplier` push. No changes needed anywhere outside `src/Ai/Raid/Naxx/`:
   `RaidStrategyContext.h:46`, `BuildSharedActionContexts.cpp:55`,
   `BuildSharedTriggerContexts.cpp:48` and the map auto-enable at `PlayerbotAI.cpp:1626` are all
   already live.

10. **`docs/raids/naxxramas/heigan-revival-findings.md`** (new) — write the gap analysis above into
    the repo before touching code, per the existing `docs/raids/naxxramas/` convention. Then update
    `naxx-threat-redirect-findings.md:106` to move Heigan out of Tier 3 (no longer blocked).

---

## Verification

Static, before running anything:

- `grep -rn "Heigan\|heigan" src/Ai/Raid/Naxx/` returns no `//`-prefixed declarations.
- Re-run the section classifier against any waypoint that gets moved: port
  `instance_naxxramas.cpp:236-252` `GetEruptionSection` into a scratch script, feed it the
  waypoints, and confirm the `idx ↔ 3 - section` mapping and the erupting-GO clearances still hold.
- Walk the index sequence by hand for both phases and confirm it matches
  `0,1,2,3,2,1,0,…` off a fresh `ResetSafe()`.

In-game (this module cannot be compiled headless here — hand the build off):

1. 10-man Naxx, plague quarter, `+naxx` strategy, full bot raid.
2. **Pull** — confirm bots do *not* enter fast-dance behaviour at the pull (G2). Tank engages,
   DPS attack, first movement happens near t+15 s.
3. **Phase 1** — bots step zone every 10 s and land in the correct wedge each time (G1). No
   eruption damage taken. Melee register damage between steps (G5).
4. **Decrepit Fever** — a Paladin/Priest/Shaman dispels, tank first.
5. **t+90 s teleport** — ranged are already off the ledge before Heigan lands, and nobody eats
   Plague Cloud (G6). Fast dance steps every 4 s, first step ~t+8 s after the teleport.
6. **t+135 s return** — bots resume Phase 1 cadence from index 0 with no stale offset (G3), and
   the threat redirect fires on the re-pull (G7).
7. **25-man** — repeat step 4 and confirm Decrepit Fever 55011 is dispelled (G4).
8. **Late joiner** — rez a bot mid-Phase-1 and confirm it holds position rather than running into
   an erupting wedge, then syncs at the next phase edge.
9. Kill the boss with no deaths; confirm the *Safety Dance* achievement still awards (the core
   fails it on any death inside the boundary, `boss_heigan.cpp:171-185`).
