# Thaddius Phase 1 (Stalagg & Feugen) — Harden

## Context

Thaddius phase 1 already has code (attack-pets action, fixed platform positions, a DPS-sync
multiplier), but it is unreliable. Two encounter rules must hold:

- **Keep each add pinned at its tesla coil.** If Stalagg/Feugen is dragged too far from its coil,
  the coil overloads and blasts the whole raid with nature damage (wipe risk).
- **Kill both adds within the revive window (~5s).** If one dies and the other is not dead within
  the window, both revive at full HP.

User decisions: **rewrite/harden the existing logic**, target the **~5s retail window**, make tanks
**hard-hold each add pinned at its coil** (never chase), and split the raid **evenly** — ≥1 tank,
~half DPS, ~half healers on each add.

Current failures:
1. **The split is not equal.** `IsAssignedToPrimarySide` sends the *first* `N` healers to the primary
   side (`N = is25Man?2:1`) and the rest to secondary, and DPS by a hardcoded count
   (`is25Man?9:3`). 10-man/3 healers → 1 vs 2; healers skew to one add. And with no RTI marks the
   split is bypassed entirely — `GetAssignedPetForBot` falls back to `GetNearestPet()` (geographic),
   so bots stack on whichever platform they spawn near. Neither guarantees ≥1 tank + ~half DPS +
   ~half healers per add, which is what makes both adds die together.
2. Pet engagement is hard-gated on RTI marks — `ThaddiusAttackNearestPetAction::isUseful()` returns
   false when the raid hasn't marked the pets (square/cross/skull), so bots ignore phase 1 entirely.
3. Tank pinning is aggro-gated — a tank only moves to the coil spot once it *has* aggro; before that
   it runs to the pet's current location, dragging it off the coil.
4. The sync rule is a threshold heuristic (`≤25/30%` + `≥4%` diff) with no hard floor, so a pet can
   still be killed while the other is well above the window → revive.

## Files to change

All under `src/Ai/Raid/Naxx/`.

- `NaxxBossHelper.h` — `ThaddiusBossHelper`: rewrite `IsAssignedToPrimarySide` for an even per-role
  split, add `GetPetForSide` so the split works mark-free, add sync constants + HP-balance helper.
- `Action/NaxxActions_Thaddius.cpp` — `ThaddiusAttackNearestPetAction`: drop the hard mark
  requirement; make tanks pin at the coil immediately on engage.
- `NaxxMultipliers.cpp` — `ThaddiusGenericMultiplier`: replace the DPS-sync block with a
  symmetric HP-balance + hard-floor rule.

No new files, no new wiring — triggers/actions/multiplier are already registered
([NaxxStrategy.cpp:113-127](../../../src/Ai/Raid/Naxx/NaxxStrategy.cpp#L113-L127),
[NaxxMultipliers.cpp:146](../../../src/Ai/Raid/Naxx/NaxxMultipliers.cpp#L146)).

## Design

### 1. Balanced role split (the core fix)

Rewrite `IsAssignedToPrimarySide(Player*)` in `NaxxBossHelper.h` so each role is split ~50/50 by
per-role index parity instead of front-loading a hardcoded count:

- **Tanks:** main tank → primary, first assist/off tank → secondary (1 per add). Reuse the existing
  `IsMainTank`/`IsAssistTank` checks and the current "tank face"/"tank assist" strategy-state
  overrides. If >2 tanks, alternate remaining tanks by tank-index parity.
- **Healers:** iterate healers in group order (loop already exists), assign
  `healerIndex % 2 == 0 → primary`, else secondary. Delete the `primaryHeals` count.
- **DPS:** iterate DPS in group order (loop already exists), assign `dpsIndex % 2 == 0 → primary`,
  else secondary. Delete the `primaryDps` count and the `is25Man` sizing.

This yields ~half DPS + ~half healers + one tank on each add regardless of raid size/comp. Keep the
final `GetGroupSlotIndex` parity fallback for anyone unclassified.

Then make the split apply **with or without RTI marks**. Add a helper
`Unit* GetPetForSide(bool primary)` returning a fixed pet per side (convention: primary = Stalagg,
secondary = Feugen), falling back to the live sibling if the chosen one is dead. In
`GetAssignedPetForBot()`, the no-mark path (currently `return GetNearestPet();` for non-tanks, and
the tank side/nearest branch) must first use `GetPetForSide(IsAssignedToPrimarySide(bot))`; drop to
`GetNearestPet()` only when neither balanced pet is available. Mark-based assignment stays the path
when an icon pair exists (marks simply pin which physical pet is "primary").

### 2. Pet engagement without RTI marks (fragility fix)

In `ThaddiusAttackNearestPetAction::isUseful()` remove the `if (!helper.HasPetIconPair()) return
false;` gate. With the balanced `GetPetForSide` path above, assignment no longer needs marks. Marks
stay the preferred split when present; absence must not disable the phase.

Keep the existing pull-safety gate (`bot->IsInCombat() || IsMainTankEngagedOnPets()`) so bots don't
pre-move off RTI marks alone.

### 3. Tank hard-hold at coil (positioning)

Tanks must pin their assigned add at the fixed coil spot from the moment they engage, not only after
aggro. In `ThaddiusAttackNearestPetAction::Execute()`, for a tank:

- Resolve the assigned pet (`GetAssignedPetForBot()` already picks the correct side, and re-picks
  after a Magnetic Pull swap via `IsOnFeugenSide(bot)`).
- Always drive the tank to `PetPhaseGetPosForTank(pet)` (the coil spot) — drop the
  `has aggro` precondition currently on
  [NaxxActions_Thaddius.cpp:72](../../../src/Ai/Raid/Naxx/Action/NaxxActions_Thaddius.cpp#L72).
  Still issue `Attack(pet)` when not already targeting it so threat is held.
- The add follows its tank via aggro, so pinning the tank at the coil pins the add.

Verify `tankPosFeugen`/`tankPosStalagg`
([NaxxBossHelper.h:1347-1348](../../../src/Ai/Raid/Naxx/NaxxBossHelper.h#L1347-L1348)) sit inside
coil leash range during in-game testing; nudge the constants toward each coil if the overload still
triggers.

Reinforce with movement suppression: in `ThaddiusGenericMultiplier` during `IsPhasePet()`,
`FollowAction` and `CombatFormationMoveAction` are already zeroed — keep that so nothing pulls the
tank off the coil.

### 4. DPS sync — symmetric balance + hard floor (5s co-death)

Replace the threshold block at
[NaxxMultipliers.cpp:182-207](../../../src/Ai/Raid/Naxx/NaxxMultipliers.cpp#L182-L207) with a rule
that keeps the two pets converging to death together. Add constants on `ThaddiusBossHelper` (e.g.
`SYNC_BALANCE_MARGIN = 5.0f`, `SYNC_HARD_FLOOR_PCT = 5.0f`, `SYNC_FLOOR_RELEASE_PCT = 8.0f`) so the
window is tunable.

For a non-tank bot whose current target is one of the two live pets, with `other` = the sibling pet:

- **Balance hold:** if `targetPct < otherPct - SYNC_BALANCE_MARGIN`, this bot is beating on the pet
  that is already lower → suppress its damage (`MeleeAction`, non-healing `CastSpellAction`) so the
  lagging pet catches up. Symmetric, active at all HP levels, margin-based — no special-casing 25/30%.
- **Hard floor (guarantees the window):** if `targetPct <= SYNC_HARD_FLOOR_PCT` and
  `otherPct > SYNC_FLOOR_RELEASE_PCT`, suppress *all* damage on this pet regardless of who targets
  it. Neither add can be pushed to 0 until the other is also within the floor band, so both drop
  through the last few % together and die inside the ~5s revive window.
- When both are within the floor band (`otherPct <= SYNC_FLOOR_RELEASE_PCT`), release all holds so
  the raid free-burns both to death simultaneously.

Tanks are never suppressed (they must keep threat/positioning). Healing is never suppressed.

Add a small `PetSyncSuppress(Unit* target)` helper on `ThaddiusBossHelper` returning the suppress
decision, so the multiplier stays readable and the thresholds live in one place.

## Verification

Cannot compile the module headless here; static verify + in-game hand-off.

Static:
- Confirm `IsAssignedToPrimarySide` splits healers/DPS by index parity — no `primaryHeals`/
  `primaryDps`/`is25Man` count logic left.
- Confirm `GetAssignedPetForBot` uses `GetPetForSide(IsAssignedToPrimarySide(bot))` before falling to
  `GetNearestPet()`.
- Grep-confirm no remaining `HasPetIconPair()` hard gate in `ThaddiusAttackNearestPetAction`.
- Confirm the multiplier no longer references the old `inSyncWindow/hardHold/softHold` locals and
  reads the new constants from the helper.
- Confirm tank branch in `Execute()` no longer requires `has aggro` to move to the coil spot.

In-game (Naxxramas, both 10N and 25N — difficulty is an instance property, logic is
difficulty-agnostic):
1. Split: on a mixed-role pull each add ends up with one tank, ~half the DPS, and ~half the healers;
   healers are not lopsided to one add. Pull **without** RTI marks → still split evenly and engage;
   pull **with** marks → split follows the marks. (fixes 1, 2)
2. Watch the two tanks: each add stays pinned at its coil for the whole phase, including after a
   Magnetic Pull tank swap; raid takes no coil nature-damage spikes. (fix 3)
3. Uneven-DPS pull: force-focus one add early → bots throttle it and let the other catch up; both
   adds die within ~5s of each other with no revive. Repeat several times. (fix 4)
