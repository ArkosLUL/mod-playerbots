# Eye of Eternity — Malygos (map 616)

Strategy key `wotlk-eoe`. Cross-raid conventions are in [README.md](README.md).

## Three plan assumptions were wrong — corrected in code

Verified against `boss_malygos.cpp`:

- **No "Deep Breath" spell exists in this core.** `SAY_DEEP_BREATH` is only the P2 Surge windup yell,
  and **`56430` is Arcane Overload**, the P2 ground void zone. `deep breath` / `deep breath dodge`
  were kept for a while as an Arcane Overload *dodge* and are now deleted — the bubbles are shelter,
  not a hazard (see P2 below).
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
- **P1 — Arcane Breath (56272)** is a frontal cone on the boss's *current victim*, so the fight is
  won or lost on where the tank stands. The tank drags Malygos to `MALYGOS_MAINTANK_POSITION`
  (42 yd due north of centre — the Exit Portal at 43.4 yd proves there is ground there), then holds
  5 yd from the boss on the far side of the line *stack point → boss*, re-issued only past 3 yd of
  drift. The raid stacks 12 yd behind that at `MALYGOS_STACK_POSITION`, inside 40 yd heal range.
  Everyone else gets `avoid arcane breath` at `ACTION_EMERGENCY + 1`, a 90° cone / 60 yd sidestep
  built on `IsBotInFrontalCone` + `GetPositionOutsideFrontalCone` (`RaidBossHelpers`).
- **P2 — the Arcane Overload bubbles are shelter, and the old code ran the wrong way.**
  `NPC_ARCANE_OVERLOAD` (30282) grants **56438, −50% damage taken**; the protected radius shrinks
  ~2 % per tick over the bubble's 45 s life, so bots hug the centre within 4 yd and ignore any bubble
  already below `BUBBLE_MIN_USABLE_FACTOR` (35 %) of its original radius. The bubble NPC is
  non-attackable, so it never shows in `"possible targets"` — scan with
  `GetCreatureListWithEntryInGrid`. `malygos seek bubble` sits at `ACTION_EMERGENCY + 2`, above
  `avoid surge of power`, which is now only the fallback for bots that cannot reach one. The bubble
  assignment is **latched by GUID** (the `SapphironFlightPositionAction` idiom) and spread by group
  slot index, or bots hop between bubbles as they shrink. Once the bot holds 56438 the action returns
  false so the rotation runs, and the phase-2 multiplier zeroes reach/chase/follow for non-vehicle
  bots so nobody walks back out; `MalygosTargetAction` only accepts a Nexus Lord / Scion inside
  `spellDistance` and otherwise leaves the bot with no target at all. Anti-fall recenter in
  `malygos position` still applies, and it bails immediately for anyone in a vehicle. Note the old
  code suppressed `FleeAction` wholesale in P2, removing the one instinct that might have pulled a
  bot back from the edge.
- **P2 hover disks — melee only.** When a Nexus Lord dies the core lands its disk (30248), turns it
  friendly and clears `UNIT_FLAG_NOT_SELECTABLE`, so *landed + selectable + free seat* doubles as
  "its rider is dead". Melee DPS board it (`EnterVehicleAction`, which uses `HandleSpellClick` —
  required, the mount is `npc_spellclick_spells` 61421) and ride it to the Scions of Eternity, which
  hover ~30 yd out and +20 yd up and are otherwise unreachable in melee. Passengers are immune to
  both Arcane Overload and Surge of Power, which is why tanks, ranged and healers stay in bubbles
  instead. `MalygosRideDiskAction` is a plain `Action` steering the vehicle's own `MotionMaster`,
  with a `POINT_MOTION_TYPE` anti-stutter guard; no dismount is needed because the core despawns
  every summon at P2→P3.
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

`group flying` and `drake combat` are borrowed from the **Oculus dungeon**, not EoE-defined. They do
resolve — `WotlkDungeonOccTriggerContext` is registered in `BuildSharedTriggerContexts.cpp:75`, so
the missing entry in `RaidEoETriggerContext` is not a bug, do not re-audit it. But
`GroupFlyingTrigger` only checks `master->GetVehicleBase() && bot->GetVehicleBase()`. If the master
is a human who has not mounted yet, bots will not fly-follow and sit on the collapsed platform.
`EnterVehicleAction` is IOC-scoped and does not help.

## Threat redirect

Three phases, three different right answers, which is why the generic main-tank node is not simply
vetoed here. See the per-raid verdicts in [ulduar.md](ulduar.md), which shares the same
investigation.
