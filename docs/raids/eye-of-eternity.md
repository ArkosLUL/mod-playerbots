# Eye of Eternity — Malygos (map 616)

Strategy key `wotlk-eoe`. Cross-raid conventions are in [README.md](README.md).

## Three plan assumptions were wrong — corrected in code

Verified against `boss_malygos.cpp`:

- **No "Deep Breath" spell exists in this core.** `SAY_DEEP_BREATH` is only the P2 Surge windup yell,
  and **`56430` is Arcane Overload**, the P2 ground void zone. The `deep breath` trigger and action
  are still wired by decision, but honestly implemented as the Arcane Overload dodge (detect/avoid
  `NPC_ARCANE_OVERLOAD 30282`, cast 56430).
- **The P2 floor is not gone** — the platform is destroyed only at P3 start
  (`EVENT_DESTROY_PLATFORM_0`). P2 anti-fall is still worth doing as recenter-on-drift, but the real
  P2 hazards are Arcane Overload and the Surge of Power beam.
- Confirmed ids: Static Field `57430` → `NPC_STATIC_FIELD 30592`; Arcane Storm `61693` (random
  target, unavoidable → heal through); Surge P2 `56505` → `NPC_SURGE_OF_POWER 30334`; Surge P3
  `57407` (10) / `60936` (25); Arcane Pulse `57432` (unavoidable); Vortex `56105` (fully
  server-driven, no bot action). Centre anchor `{754.395, 1301.27, 266.1}`.

## Phase model

P1 = Malygos attackable and no adds. P2 = disc adds (`NPC_NEXUS_LORD` / `NPC_SCION_OF_ETERNITY`) up.
P3 = on a Wyrmrest Skytalon (30161). **P4 = transition** — in combat, Malygos non-attackable, not
mounted.

P4 exists because of a real failure: during P2→P3 the boss is untargetable and the bot is not yet on
its drake, so the phase resolved to 0, `MalygosMultiplier` lifted every override, and **the default
raid strategy resumed mid-transition on a collapsing platform**. P4 holds the raid at centre and
keeps the EoE overrides on.

`getMalygos` uses `FindNearestCreature` so detection survives the non-attackable flag.

## Per-phase behaviour

- **P1 — Power Sparks.** Previously fully stubbed: the trigger was registered but never bound, the DK
  death-grip pull was commented out, the ranged target-switch was commented out, and
  `KillPowerSparkAction` had no body and no registration. Sparks reaching Malygos stack a
  damage + haste buff (56152) into a soft enrage. Now: `power spark` bound, DK `pull power spark`
  at +3, everyone else `kill power spark` at +2, and ranged also switch in `malygos target`.
- **P2** — anti-fall recenter in `malygos position`, plus the Arcane Overload dodge and
  `avoid surge of power`. Note the old code suppressed `FleeAction` wholesale in P2, removing the one
  instinct that might have pulled a bot back from the edge.
- **P3** — drake formation fanned to **15 yd** (was `MoveFollow(masterVehicle, 3.0f)`, which stacked
  every drake inside Static Field and Arcane Storm). Added `avoid static field` and
  `drake dodge surge` (Flame Shield 57108 + Blazing Speed 57092 + strafe), both of which were enum'd
  but never cast. `DrakeHealAction` now heals the **most-injured ally drake** (Revivify single / Life
  Burst AoE) instead of only its own vehicle, and the heal role is picked by `IsHeal` with a
  count-based minimum fallback rather than a GUID sort. DPS drakes range-close to the boss — the old
  engage branch was `if (boss && false)`, dead code.

**Drake avoid and attack actions are plain `Action`, not `MovementAction`**, so the P3 movement
suppression leaves them free.

## Fragile by design

`group flying` and `drake combat` are borrowed from the **Oculus dungeon**, not EoE-defined, and
`GroupFlyingTrigger` only checks `master->GetVehicleBase() && bot->GetVehicleBase()`. If the master
is a human who has not mounted yet, bots will not fly-follow and sit on the collapsed platform.
`EnterVehicleAction` is IOC-scoped and does not help.

## Threat redirect

Three phases, three different right answers, which is why the generic main-tank node is not simply
vetoed here. See the per-raid verdicts in [ulduar.md](ulduar.md), which shares the same
investigation.
