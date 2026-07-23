# Kel'Thuzad (Naxx) strategy — fix 3 raid-wipe gaps

## Context

User-observed wipes on KT with playerbots:
1. **P1**: tanks/melee pull idle adds parked at room edge (should only fight adds actually heading in).
2. **Shadow Fissure**: targeted bot escapes, bystanders standing in same spot die to Void Blast.
3. **P2 Frost Blast** (10y chain around target) hits large chunks of the raid due to clumped positioning.

### Root causes (verified in code)

- **P1 pulls** — `KelthuzadChooseTargetAction::Execute` (`src/Ai/Raid/Naxx/Action/NaxxActions_Kelthuzad.cpp:23-67`): candidate filter = within 30y of room center + spellDistance of bot. **No activity check** (in combat / has victim / moving). Parked perimeter adds inside 30y are targetable; ranged P1 list has no center gate at all.
- **Fissure** — escape at `NaxxActions_Kelthuzad.cpp:281-297`: dest = fissure + exactly 10y (Void Blast 27812 radius = 10y + target combat reach → still hit); `ClampToRoom` projects outward escapes back onto the ≤24y circle, often back inside blast radius (outer-ring ranged oscillate in place). Fissure check is step 4 in the P2 chain — bots handling Detonate/near-FB return at earlier steps and can walk into fissures; `GetAnyShadowFissure` returns last-iterated, not nearest.
- **Frost Blast** — `ComputeRangedSpreadPosition` (`src/Ai/Raid/Naxx/NaxxBossHelper.h:279-331`): rings 18/21/24 only 3y apart → cross-ring neighbors ~6.6y when 11+ ranged (25-man). Melee DPS have **no P2 positioning** — stack behind boss within 10y of each other and usually the MT. Reactive move-away uses 8y < 10y chain radius.
- **Bonus bugs found**: (a) Detonate Mana dest clamps into 20-24y band = exactly the ranged ring → detonating melee explodes on ranged slots; (b) 25-man interrupt boost dead — `NaxxMultipliers.cpp:326` checks Frost Bolt 28478 only; 25-man id is **55802** (spelldifficulty_dbc: 28478→55802, 28479→55807).

### Verified facts

- `unit->isMoving()` exists (Unit.h:1712, `MOVEMENTFLAG_MASK_MOVING`); `GetCombatReach()`, `GetMeleeRange()` exist.
- FrostBlast 27808 / DetonateMana 27819 / Chains 28410 / ShadowFissure 27810 / VoidBlast 27812 have **no** 25-man variants — single-id checks OK. Only Frost Bolt has variants.
- Shadow Fissure NPC entry = **16129**; `"nearest triggers"` value range = sightDistance (detection range fine).
- P1 activation model (core `boss_kelthuzad.cpp`): activated adds are `AttackStart()`ed immediately (in combat + victim + moving); parked adds are stationary decoration with proximity aggro. Predicate `IsInCombat() || GetVictim() || isMoving()` discriminates cleanly. At P2 transition non-combat minions despawn → filter can't starve P2 targeting.
- `botAI->GetMeleeIndex()` counts tanks too — need per-boss melee-DPS-only index helper.
- Emergency-dodge convention: `ACTION_EMERGENCY + 6`, `MOVEMENT_FORCED` (ZA precedent `src/Ai/Raid/ZA/ZAActions.cpp:106`).
- MT hold `tank_pos` is 7.21y from center, world angle ≈166°.

### User decisions

- Melee rear-arc spread: **yes**.
- Detonate Mana angle-optimization fix: **yes**.
- Decided in-plan: keep ROOM_MAX_RADIUS=24 (heal-range budget); inner residual ranged slots ordered to protect MT over melee.

---

## Implementation

### Step 1 — `KelthuzadBossHelper` additions (`src/Ai/Raid/Naxx/NaxxBossHelper.h:104-564`)

New constants:
```cpp
static constexpr uint32 NPC_SHADOW_FISSURE = 16129;
static constexpr float FISSURE_DANGER_RADIUS = 13.0f;   // Void Blast 10y + reach + margin
static constexpr float FROST_BLAST_SAFE_DIST = 12.0f;   // chain radius 10y + margin
static constexpr float P1_ACTIVE_ADD_MAX_CENTER_DIST = 45.0f;
```

New methods:
1. `bool IsAddActive(Unit* unit) const` — `unit && (unit->IsInCombat() || unit->GetVictim() || unit->isMoving())`.
2. `Unit* GetNearestShadowFissure()` — replaces `GetAnyShadowFissure` (h:543-558). Match `Creature::GetEntry() == NPC_SHADOW_FISSURE` with name fallback (mirror `IsGuardian` h:111-119); keep minimum `bot->GetDistance2d`. Update the one call site.
3. `bool IsNearShadowFissure(float x, float y, float radius = FISSURE_DANGER_RADIUS)` — any fissure within radius of (x,y); used to veto movement destinations.
4. `bool ComputeEscapeFromPoint(float hx, float hy, float safeDist, float& outX, float& outY)` — shared clamp-aware escape:
   - Radial: `dir = normalize(botPos − hazard)` (degenerate → `normalize(botPos − center)`, then +X); `dest = hazard + dir·(safeDist+1)`; `ClampToRoom`; accept if `dist2d(dest, hazard) ≥ safeDist`.
   - Tangential fallback (always exists on ring): `r = clamp(bot radius, ROOM_MIN, ROOM_MAX)`; try `theta ± k·(π/12)`, k=1..8, away-sign first, at radius r; first candidate ≥ safeDist from hazard wins.
5. `uint32 GetMeleeDpsIndex(Player*)` / `uint32 GetMeleeDpsCount()` — group-order iteration (mirror `GetRangedCount` h:171-194), counting `!IsRanged && !IsTank` only.
6. `bool IsBossCastingAny(std::initializer_list<uint32>)` — generalize `IsBossCasting` (h:154-169); keep single-id overload delegating.
7. **Rewrite `ComputeRangedSpreadPosition`** — two-ring layout, angles relative to `tankAngle = atan2(tank_pos − center)` computed at runtime:
   - **Outer ring r=24**: `nOuter = min(total, 12)` slots at `15° + k·(360°/nOuter)`. N=12 chord 12.42y ≥ 11 ✓; worst slot-to-MT 17.1y ✓.
   - **Inner ring r=14** (only total > 12): fixed relative-angle candidates `{+90°, −90°, +150°, −150°, +30°, −30°}` (staggered 15° off outer 30° grid; ±90/±150 first = protect MT; ±30 are 8.6y from MT = last-resort residual slots for 15th+ ranged). Inner-to-outer min 11.09y ✓ (why r=14 not 13); inner-inner ≥14y ✓.
   - Keep final `ClampToRoom` and caller's 2y arrival tolerance.
8. `bool ComputeMeleeSpreadPosition(uint32 index, uint32 total, float& outX, float& outY)` — melee rear-arc:
   - `boss = GetBoss()`; MT = first alive `IsMainTank` member (fallback: tank_pos direction). `refAngle = boss->GetAngle(mt)`.
   - Radius `r = clamp(boss->GetCombatReach() + 1.0f, 5.0f, bot->GetMeleeRange(boss) − 0.5f)`.
   - MT-exclusion half-arc `phi = max(100°, 2·asin(min(1, 5.5/r)))` → every slot ≥11y from MT when r ≥ ~6.4 (r=9 → 13.8y). If r < 5.5, isolation impossible: phi=π, single rear point, accept residual.
   - Slots evenly across `[refAngle+phi, refAngle+2π−phi]`; total==1 → `refAngle+π`. Boss-relative position, `ClampToRoom`.

### Step 2 — P1 activity filter (`NaxxActions_Kelthuzad.cpp:23-36`)

Restructure filters in bucket loop:
```cpp
bool isKelthuzad = botAI->EqualLowercaseName(unit->GetName(), "kel'thuzad");
if (isKelthuzad)
{
    if (unit->GetDistance2d(center) > 30.0f) continue;
}
else
{
    if (!helper.IsAddActive(unit)) continue;              // never target parked adds
    if (unit->GetDistance2d(center) > P1_ACTIVE_ADD_MAX_CENTER_DIST) continue;  // engage incoming from alcoves
}
if (bot->GetDistance2d(unit) > sPlayerbotAIConfig.spellDistance) continue;
```
Melee 20y center gate (line 102) stays — that's the body-pull mitigation.

### Step 3 — Fissure emergency flee (7-component pattern)

- **Trigger** `KelthuzadShadowFissureTrigger` ("kel'thuzad shadow fissure") in `NaxxTriggers.{h,cpp}`: `UpdateBossAI()` && nearest fissure && `bot->IsWithinDistInMap(fissure, FISSURE_DANGER_RADIUS)`.
- **Action** `KelthuzadFleeShadowFissureAction` ("kel'thuzad flee shadow fissure") in `NaxxActions.h` + `NaxxActions_Kelthuzad.cpp`: get nearest fissure; if within danger radius → `ComputeEscapeFromPoint`; `MoveTo(..., MovementPriority::MOVEMENT_FORCED, true, false)`.
- **Register** in `NaxxTriggerContext.h` + `NaxxActionContext.h` (existing creators pattern).
- **Wire** in `NaxxStrategy.cpp` after KT node (:38-45): `TriggerNode("kel'thuzad shadow fissure", { NextAction("kel'thuzad flee shadow fissure", ACTION_EMERGENCY + 6) })` — priority 96 beats all P2 positioning (62), fixing ordering by construction.
- **Remove** old fissure branch from `KelthuzadPositionAction` (delete else-block :281-297, drop fissure condition :217-218 so role positioning runs unconditionally).
- **Guard destinations**: `if (helper.IsNearShadowFissure(dx, dy)) return false;` after computing ranged slot, FB-spread dest, detonate dest, MT hold move, melee slot.

### Step 4 — Frost Blast reactive spread (`NaxxActions_Kelthuzad.cpp:208-216`)

Replace with: proximity `< FROST_BLAST_SAFE_DIST` (12y), escape via `ComputeEscapeFromPoint(fbTarget pos, FROST_BLAST_SAFE_DIST, ...)`, fissure-check dest, `MOVEMENT_COMBAT`.

### Step 5 — Melee spread + Detonate placement (P2 branch of `KelthuzadPositionAction`)

- **Melee**: add final `else` (plain melee DPS, currently no-op): `GetMeleeDpsIndex/Count` → `ComputeMeleeSpreadPosition` → fissure-guard → 2.5y arrival hysteresis (boss moves) → `MoveTo(MOVEMENT_COMBAT)`.
- **Detonate** (replace :188-201): sample 24 angles (15° steps) at `DETONATE_MAX_RADIUS`; score = min dist to every other alive group member; discard `IsNearShadowFissure` candidates; among scores within 1y of best pick closest to bot (least travel — blast fires ~5s after application); `ClampToRoom(dx, dy, DETONATE_MIN, DETONATE_MAX)`.

### Step 6 — Hunter Misdirection on main tank

Generic wiring exists (`GenericHunterStrategy.cpp:68`: "low tank threat" → "misdirection on main tank" @27) but is unreliable mid-encounter; ZA raids use an explicit per-boss action (`ZAActions.cpp:34-52`, `AkilzonMisdirectBossToMainTankAction : AttackAction`). Mirror that:

- **Action** `KelthuzadMisdirectBossToMainTankAction : AttackAction` ("kel'thuzad misdirect boss to main tank") in `NaxxActions.h` + `NaxxActions_Kelthuzad.cpp`:
  - `helper.UpdateBossAI()` gate; MT via `GetGroupMainTank(botAI, bot)` (`RaidBossHelpers.h` — add include, Naxx doesn't use it yet); return false if none.
  - `if (botAI->CanCastSpell("misdirection", mainTank)) return botAI->CastSpell("misdirection", mainTank);` — non-hunters/CD fail harmlessly.
  - If self has Misdirection aura (34477): threat transfer shot — P2: `steady shot` at boss; P1: at current active add target if any. (`bot->HasAura(NaxxSpellIds::Misdirection)` + `CanCastSpell("steady shot", target)`.)
  - Works both phases: P1 dumps add threat onto MT, P2 secures fresh threat table at transition.
- **Register** in `NaxxActionContext.h`; **wire** into the existing "kel'thuzad" TriggerNode (`NaxxStrategy.cpp:38-45`) as `NextAction("kel'thuzad misdirect boss to main tank", ACTION_RAID + 3)` (above position 62 — falls through when not castable).
- **Spell id**: `NaxxSpellIds.h`: `static constexpr uint32 Misdirection = 34477;`.

### Step 7 — Multiplier (`NaxxMultipliers.cpp:296-421`)

1. Add `KelthuzadFleeShadowFissureAction` to healer allowlists in FrostBlast-in-group (:349-360) and Chains-in-group (:361-372) branches — else healers pinned in fissures during those.
2. Interrupt boost (:326): `IsBossCastingAny({FrostBoltSingle, FrostBoltSingle25})`.

### Step 8 — Spell ids (`NaxxSpellIds.h:91-97`)

```cpp
static constexpr uint32 FrostBoltSingle25 = 55802;
static constexpr uint32 FrostBoltMulti25  = 55807;
static constexpr uint32 VoidBlast = 27812;   // fissure damage, 10y radius
static constexpr uint32 Misdirection = 34477;
```

### Order

1. Step 1 (helper) → 2. Steps 8+7 → 3. Step 2 → 4. Step 3 → 5. Step 4 → 6. Step 5 → 7. Step 6 → 8. `python apps/codestyle/codestyle-cpp.py`. **No build** (user builds separately).

## Verification (in-game, 10-man + 25-man)

1. P1: no bot attacks/moves toward stationary perimeter adds; ranged open on freshly activated adds walking in (>30y); melee engage only ≤20y from center.
2. Fissure: any bot within 13y (incl. detonate-carrier, FB-adjacent, healers mid-FB) escapes within ~5s window; outer-ring bots escape tangentially, never clamp-trapped.
3. Ranged at rest pairwise ≥11y with ≤14 ranged; one Frost Blast never hits two ranged.
4. Melee in rear arc, ≥11y from MT; FB on melee never freezes MT.
5. Detonate carrier lands in gap ≥ raid distance maximized; no ranged casualties.
6. 25-man interrupt boost fires on Frost Bolt 55802.
7. Hunter bots cast Misdirection on MT when off CD (P1 add waves + P2 start), then steady shot to transfer threat; non-hunters unaffected.
8. Regressions: off-tank guardian pickup, MT hold, chains AttackStop, pet recall unchanged.

## Runtime unknowns (check in-game, not blockers)

- KT effective `GetCombatReach()` (melee slot radius; MT isolation guaranteed only if r ≥ ~6.4y).
- Parked adds report `isMoving() == false` (expected; OR-predicate robust even if in-combat linking differs).
