# Malygos (Eye of Eternity) Bot Strategy — Wipe-Risk Audit + Remediation Plan

## Implementation status (2026-07-24)

Implemented across `src/Ai/Raid/EoE/`. Not yet compiled (headless build isn't possible in
this sandbox) — needs a server build + live pull to confirm.

Spell IDs were verified against the core script
(`src/server/scripts/Northrend/Nexus/EyeOfEternity/boss_malygos.cpp`). Three plan assumptions
were wrong and are corrected in code:

- **No "Deep Breath" spell exists in this core.** `SAY_DEEP_BREATH` is only the P2 Surge
  windup yell, and `56430` (the plan's Deep Breath id) is actually **Arcane Overload**, the P2
  ground void zone. Per the user's decision the "deep breath" trigger/action are still wired,
  but honestly implemented as the Arcane Overload dodge (detect/avoid `NPC_ARCANE_OVERLOAD
  30282` / cast `56430`).
- **The P2 floor is not gone** — the platform is destroyed only at P3 start
  (`EVENT_DESTROY_PLATFORM_0`). P2 anti-fall is still done (recenter drift) but the real P2
  hazards are Arcane Overload + Surge of Power beam.
- **Confirmed ids:** Static Field `57430` → `NPC_STATIC_FIELD 30592`; Arcane Storm `61693`
  (random-target, unavoidable → heal through); Surge P2 `56505` → `NPC_SURGE_OF_POWER 30334`;
  Surge P3 `57407` (10) / `60936` (25); Arcane Pulse `57432` (unavoidable); Vortex `56105`
  (fully server-driven, no bot action). Center anchor `CenterPos {754.395, 1301.27, 266.1}`.

Phase model is now: `getPhase` P1 = Malygos attackable + no adds; P2 = disc adds
(`NPC_NEXUS_LORD`/`NPC_SCION_OF_ETERNITY`) up; P3 = on a Skytalon drake; **P4 = transition**
(in combat, Malygos non-attackable, not mounted) — P4 holds the raid at centre and keeps EoE
overrides on so the default strategy never resumes on the collapsing platform. `getMalygos`
uses `FindNearestCreature` so detection survives the non-attackable flag.

What each gap became: P1 sparks — `power spark` trigger bound, DK `pull power spark`
(death-grip, +3) + everyone-else `kill power spark` (+2), ranged also switch in
`malygos target`. P2 — anti-fall recenter in `malygos position`, `deep breath dodge` (Arcane
Overload), `avoid surge of power`. P3 — drake formation fanned to 15yd (was 3yd),
`avoid static field` + `drake dodge surge` (Flame Shield `57108` + Blazing Speed `57092`
+ strafe), `DrakeHealAction` now heals the most-injured ally drake (Revivify single /
Life Burst AoE), heal-role picked by `IsHeal` with a count-based minimum fallback, DPS drakes
range-close to the boss. Drake avoid/attack actions are plain `Action` (not `MovementAction`)
so the P3 movement suppression leaves them free.

## Context

The playerbot strategy for Malygos (Eye of Eternity, map 616, strategy `wotlk-eoe`) is
skeletal. All logic lives in five files under `src/Ai/Raid/EoE/`. Large parts of the
intended behaviour are commented out or dead-coded, and every phase has survival mechanics
with **zero** bot coverage. When bots run this encounter today they reliably wipe on
mechanics they never react to — most severely in Phase 3 (Deep Breath one-shot, Static
Field, Arcane Storm) and in Phase 1 (Power Sparks are completely unhandled and buff the
boss to a soft enrage).

This document is the gap audit plus a remediation plan covering **all** wipe-risk gaps
across P1/P2/P3 and the phase transitions.

## How the strategy is wired today

- Auto-assigned on map 616 → `wotlk-eoe` — `src/Bot/PlayerbotAI.cpp:1737`. Factory: `RaidStrategyContext.h:46,70`.
- Only **4** trigger nodes fire — `src/Ai/Raid/EoE/EoEStrategy.cpp:5-16`:
  - `malygos` → `malygos position` (ACTION_MOVE) + `malygos target` (ACTION_RAID+1)
  - `group flying` → `eoe fly drake` (ACTION_NORMAL+1)
  - `drake combat` → `eoe drake attack` (ACTION_NORMAL+5)
- `group flying` / `drake combat` are **borrowed from the Oculus dungeon** (`src/Ai/Dungeon/OC/OCTriggers.cpp:42-54`), not EoE-defined. `GroupFlyingTrigger` only checks `master->GetVehicleBase() && bot->GetVehicleBase()`.
- Phase model is derived, not event-driven — `src/Ai/Raid/EoE/EoETriggers.cpp:5-25`: P3 = on `NPC_WYRMREST_SKYTALON` (30161) vehicle; else P1 if boss >50% HP; else P2; else P0 (no handling).
- Vehicle-cast framework exists and is usable: `PlayerbotAI::CanCastVehicleSpell` / `CastVehicleSpell` (`src/Bot/PlayerbotAI.cpp:3994,4079`), generic `CastVehicleSpellAction` (`GenericSpellActions.h:399`).
- Reusable avoidance precedent: OC `avoid unstable sphere` / `avoid arcane explosion` / `time bomb spread` (`OCStrategy.cpp:8-30`); ICC BPC vortex-spread (`ICCActions_BPC.cpp`). Base `AvoidAoeAction` exists and is **not** referenced by EoE.

## Gap audit — ranked by wipe risk

### CRITICAL (direct wipe, zero coverage)

1. **P3 Deep Breath** — Malygos's signature drake one-shot. No trigger, no spell ID, no
   dodge. Bots formation-follow the master regardless. `src/Ai/Raid/EoE/EoEActions.cpp:232-274`.
2. **P3 Static Field** — persistent AoE; drakes must spread. Formation flight stacks all
   drakes at radius 3yd around the master (`MoveFollow(masterVehicle, 3.0f, angle)`,
   `EoEActions.cpp:269`) → everyone eats it.
3. **P3 Arcane Storm** — raid-wide pulses. Drake healers only heal their **own** vehicle
   (`DrakeHealAction` targets `vehicleBase` only; Revivify never targets injured allies —
   `EoEActions.cpp:381-402`). No storm heal ramp.
4. **P1 Power Sparks — fully stubbed.** `PowerSparkTrigger` is registered but **never
   bound** in `InitTriggers`. DK death-grip pull (`PullPowerSparkAction`) is entirely
   commented out; ranged-DPS spark target-switch commented out; `KillPowerSparkAction` has
   no body and is unregistered. `EoEActions.cpp:40-49,81-97,154-225`. Sparks walk into
   Malygos → stacking damage+haste buff (56152) → soft enrage → wipe.

### HIGH

5. **P2 no positioning / fall deaths.** `MalygosPositionAction` only handles phase 1 —
   returns false in P2. The floor is gone (floating discs); default combat movement chases
   adds and can walk bots off the edge into the void. `FleeAction` is suppressed in P2
   (`EoEMultipliers.cpp:59-62`), removing the one instinct that might pull them back.
6. **P2/P3 Surge of Power** — beam mechanic; no line-of-sight/spread/shield reaction.
7. **Drake-mount dependency is fragile.** No EoE vehicle-entry action; bots rely on the
   encounter auto-seating them. `group flying` needs the **master** on a vehicle — if the
   master is a human not yet mounted (or dismounted), bots won't fly-follow and sit on the
   collapsed platform. `EnterVehicleAction` is IOC-scoped and won't help (`VehicleActions.cpp:18-21`).
8. **Phase-transition gap.** During P2→P3 the boss is untargetable and the bot isn't yet on
   the drake → `getPhase` returns **0** → `MalygosMultiplier` lifts every override → the
   default raid strategy resumes mid-transition (random targeting/movement on a floorless map).

### MEDIUM

9. **Drake heal role picked by GUID sort, not real role** (`EoEActions.cpp:315-349`) — the wrong bots may heal/DPS.
10. **Drake utility CDs unused.** `SPELL_FLAME_SHIELD (57108)` and `SPELL_BLAZING_SPEED (57092)` are enum'd but never cast — these are the Deep-Breath / Static-Field mitigation tools (gaps 1-2).
11. **Arcane Breath cone** only implicitly handled via fixed tank/stack coords; no active cone-dodge, brittle if positions drift.
12. **Drake never navigates to boss.** The engage branch is dead (`if (boss && false)`, `EoEActions.cpp:242`); DPS only lands if the master happens to fly into range.

### LOW

13. **Vortex (P1)** — mostly server-driven vehicle mechanic; healers should keep casting through it. Low wipe contribution, worth a heal-continue guard only.

## Remediation plan

Work stays inside `src/Ai/Raid/EoE/` plus the 4 standard wiring sites (see the
`raid-boss-strategy-recipe` skill). New triggers/actions get registered in
`EoETriggerContext.h` / `EoEActionContext.h` and bound in `EoEStrategy.cpp`.
**First task for the implementer: confirm the exact spell IDs below against
`SpellMgr`/DBC or the server's `boss_malygos` SmartAI/script** — the enum in
`EoETriggers.h:8-39` only has P1/P2/drake IDs; P3 hazard IDs are absent. Candidate IDs
to verify (do not hardcode until confirmed): Deep Breath ~56430, Static Field ~57063
(H 60122), Arcane Storm ~57459/61693, Surge of Power (P2) ~56505 / (P3) ~57407, Vortex ~56105.

### P1 — restore Power Spark handling

- Add spell IDs to the enum once confirmed; give `NPC_POWER_SPARK (30084)` first-class targeting.
- **Bind the existing `PowerSparkTrigger`** in `InitTriggers` (it is already implemented and
  registered — just unbound). Un-comment / finish the DK death-grip pull path and the
  ranged-DPS target-switch in `MalygosTargetAction`/`MalygosPositionAction`. Reuse the
  commented `PullPowerSparkAction` body (`EoEActions.cpp:154-220`) as the starting point;
  register it (uncomment `EoEActionContext.h:15,24`).
- Priority: spark handling must outrank plain boss tunnel (place its NextAction above
  `malygos target`, e.g. ACTION_RAID+3).
- Keep the tank-away geometry for Arcane Breath; add an explicit "melee stay behind boss"
  clause instead of relying only on the fixed stack point.

### P2 — anti-fall + Surge of Power

- Add a **phase-2 branch to `MalygosPositionAction`**: keep bots at a safe interior radius
  (clamp to a disc-center anchor; never path across the void). Do **not** suppress
  `FleeAction` toward the void — replace the blanket suppression at
  `EoEMultipliers.cpp:59-62` with a directional/anchored constraint.
- Add a `surge of power` trigger (boss casting the P2 Surge spell) → an avoid/LoS action
  modelled on OC `avoid arcane explosion` (`OCStrategy.cpp:27`) / `AvoidAoeAction`.

### P3 — the big one (Deep Breath / Static Field / Arcane Storm / Surge)

- **Spread the formation.** Change `EoEFlyDrakeAction` from `MoveFollow(master, 3.0f, …)`
  to a wider per-slot radius (≥15–20yd) so Static Field / Arcane Storm don't hit the whole
  flight. Keep the slot-angle fan from `GetGroupSlotIndex`.
- **Deep Breath dodge:** new `deep breath` trigger (detect Malygos casting the Deep Breath
  spell, or the telegraph). Bound to a high-priority drake-move action that flies the
  vehicle perpendicular/out of the breath line. Use `vehicleBase->GetMotionMaster()` as the
  existing flight code does; gate it above formation-follow priority.
- **Arcane Storm heal ramp:** in `DrakeHealAction`, target injured **ally** drakes with
  Revivify (single-target) and use `Life Burst` (AoE, combo≥5) when multiple drakes are hurt
  — currently it only heals `vehicleBase`. Pull group drakes via the group iteration already
  present at `EoEActions.cpp:318-337`.
- **Use the utility CDs:** cast `SPELL_FLAME_SHIELD (57108)` reactively (incoming Static
  Field / Deep Breath) and `SPELL_BLAZING_SPEED (57092)` to escape — both currently unused.
- **Fix heal-role selection:** replace the GUID-sort split (`EoEActions.cpp:315-349`) with
  real role (`botAI->IsHeal(bot)`), falling back to the count-based split only to guarantee
  a minimum number of drake healers.
- **Re-enable active boss navigation:** replace the dead `if (boss && false)` branch
  (`EoEActions.cpp:242`) with real range-closing to ~55yd so DPS lands even if the master
  idles — but keep it subordinate to Deep Breath dodge and spread.

### Phase-transition robustness

- Harden `MalygosTrigger::getPhase` so the P2→P3 gap doesn't fall to phase 0: treat
  "boss present but untargetable/in-air, not yet drake-mounted" as a transition phase that
  **keeps EoE overrides active** (suppress default follow/assist/movement) and holds
  position rather than reverting to the generic strategy.
- Add a drake-mount safety net: if in P3 conditions but not seated, attempt to enter the
  nearest `NPC_WYRMREST_SKYTALON`, and make `group flying` tolerate the master being a human
  who mounts a beat later (don't hard-require `master->GetVehicleBase()` — fall back to
  "self on drake").

## Files to modify

- `src/Ai/Raid/EoE/EoETriggers.h` — add P3 hazard NPC/spell IDs (confirmed), new trigger classes.
- `src/Ai/Raid/EoE/EoETriggers.cpp` — `getPhase` transition phase; new trigger `IsActive()` bodies; keep `PowerSparkTrigger`.
- `src/Ai/Raid/EoE/EoEActions.h/.cpp` — P2 positioning branch; Deep-Breath dodge action; spread radius; ally-drake healing; utility CDs; role fix; re-enable boss navigation; finish Power Spark actions.
- `src/Ai/Raid/EoE/EoEActionContext.h` / `EoETriggerContext.h` — register the new + revived actions/triggers.
- `src/Ai/Raid/EoE/EoEStrategy.cpp` — bind `power spark`, `surge of power`, `deep breath`, P2-position, transition-hold triggers with correct priorities.
- `src/Ai/Raid/EoE/EoEMultipliers.cpp` — P2 anti-fall constraint; keep P3 movement gating but allow the new Deep-Breath dodge action through.

Follow the 7-file component layout and 4 wiring sites in the `raid-boss-strategy-recipe`
skill. Handle heroic (25H) via the existing `GetRaidDifficulty()` branches already present
in the drake code; use the `_N`/`_H` spell-ID pairs for hazards.

## Verification

Static: build the module (cannot compile headless in the dev sandbox — hand off the build,
or do a structural verify). Confirm all new triggers/actions are registered in both contexts
and bound in `EoEStrategy.cpp`, and that no `if (… && false)` / commented dead paths remain
for the mechanics we now cover.

Live (in-server, with a bot raid on map 616):
- **P1:** spawn/observe Power Sparks — bots switch to kill/grip them before they reach
  Malygos; boss gains no Power Spark stacks; tank holds `{757,1337}`, raid stacked behind.
- **P2:** collapse to discs — no bot falls into the void; adds split correctly
  (ranged→Scions, melee→Nexus Lords); bots react to Surge of Power.
- **P3:** all bots mount drakes at transition (no phase-0 revert); drakes spread ≥15yd (no
  mass Static Field/Arcane Storm deaths); on Deep Breath cast the flight clears the breath
  line; injured drakes get healed by others; Malygos dies without a drake-death cascade.
- Regression: bots entering map 616 still auto-apply `wotlk-eoe` and behave normally in P0
  (trash / pre-pull).
