# Flame Leviathan: why the vehicle fleet dies

## Status

Steps 1–5 are implemented and unverified — the module cannot be built here, so nothing below has
been compiled or run. Step 6 (pyrite cadence) is deliberately not started: it needs a trace first,
and the `fl.pyrite` probe that produces one landed in step 1.

Two things in the plan body turned out to be wrong once the core was read, and the code follows the
corrected version:

- **The cone widths are per spell, not 90°.** `Spell::SelectImplicitConeTargets` defaults to 60° and
  `world.spell_cone` overrides it: Ram 100°, Electroshock 60°, Sonic Horn 50°. `isInFront` passes no
  target radius, so his 15 yd reach does not widen them. `CAST_ANGLE_IN_FRONT` is 120°, wider than
  all three, which is why `CastVehicleSpell` never turned the vehicle and why Ram (100°) landed 63%
  while Electroshock (60°) landed 1.4% from the same vehicle. **This is the root cause**, and the
  distance mismatch was secondary.
- **Do not lower `ULDUAR_FL_SIEGE_STAND_DIST`.** The cone range check adds the *target's* combat
  reach, so Ram's 18 yd reaches 33 yd against him and a siege engine parked at 23 yd was always in
  range. Distance was never the Ram problem. `ULDUAR_FL_CHOPPER_STAND_DIST` did move (6 → 20), for a
  different reason: 6 put a chopper 21 yd from his centre, permanently inside Battering Ram's 25.

## Context

Five Flame Leviathan pulls on 2026-08-29 (18:19–19:23 local, Ulduar map 603 instance 1) all wiped —
the boss evaded at the end of every one and never died. The user asked why bots died, whether
demolishers held 10 stacks of pyrite, and why Electroshock never seems to interrupt Flame Vents, then
narrowed the death question to **vehicle** losses rather than on-foot bot deaths (bots on foot are
already a lost fight — they get there because their vehicle was destroyed).

There is no RaidObs trace for any of these pulls, so the whole analysis below comes from the
mod-chronicle combat log `env/dist/logs/chronicle_logs/instance_603_1_1788031301.log`. That missing
trace is itself finding #1: it is why there is no position or decision data to work from.

Two towers (Storms + Frost) stood for all five attempts, so the boss carried two tower buffs and the
arena ran Thorim's Hammer and Hodir's Fury.

## What the log says

**Attempts** (all wipes, boss evaded each time; attempt 1 was a botched 1.4 min start):

| # | Window (epoch ms) | Length | Damage on boss |
|---|---|---|---|
| 2 | 1788018188517–1788018501784 | 5.2 min | 96.6 M |
| 3 | 1788018628781–1788018908048 | 4.7 min | 81.3 M |
| 4 | 1788019152171–1788019470233 | 5.3 min | 88.3 M |
| 5 | 1788020298184–1788020581645 | 4.7 min | 38.0 M |

**60 vehicles destroyed across attempts 2–5.** Ranked by damage taken (47.5 M total on
Salvaged Siege Engine / Demolisher / Chopper — HP 1 134 000 / 630 000 / 509 040):

| Source | Damage | Share | Hits | Killing blows |
|---|---|---|---|---|
| Flame Vents | 18.5 M | 39 % | 5263 | 21 |
| Battering Ram | 14.0 M | 29.5 % | 284 | 20 |
| Missile Barrage | 6.6 M | 13.8 % | 1688 | 6 |
| Hodir's Fury | 5.2 M | 10.8 % | 65 | 9 |
| Thorim's Hammer | 3.2 M | 6.7 % | 817 | 3 |

Missile Barrage has no counterplay in the AC script. Everything else is avoidable and is not being
avoided.

### The geometry is wrong, and it explains most of it

`creature_template_model` → `creature_model_info`: **Flame Leviathan's CombatReach is 15.0**, the
Salvaged Siege Engine's is 7.7.

`FlameLeviathanOffsetPoint` ([UldBossHelper.cpp:2728](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L2728))
parks at `boss->GetCombatReach() + standDist`, so with `ULDUAR_FL_SIEGE_STAND_DIST = 8` a siege engine
sits **23 yd from the boss's centre**, a chopper at 21 yd, a demolisher at 65 yd. Against the DBC:

- **Battering Ram (62376)** is a 25 yd radius blast (RadiusIndex 20) on a dest in front of the boss.
  Siege engines and choppers park *inside* it by construction.
- **Ram (62345)**, the siege engine's own attack, is an 18 yd cone (RadiusIndex 19). A siege engine
  parked at 23 yd is *outside its own weapon's range*. 538 Ram casts, 339 landed (63 %) — 199 casts
  × 40 energy thrown away.
- **Electroshock (62522)** is a 25 yd cone (RadiusIndex 20), 90° arc, RangeIndex 1 (max range 0),
  10 s cooldown, 20 energy, Effect_1 = `SPELL_EFFECT_INTERRUPT_CAST`.

The eligibility tests use `IsWithinCombatRange`, which adds **both** combat reaches:
`IsWithinCombatRange(boss, 25)` is true out to 25 + 15 + 7.7 = **47.7 yd**, while the cone itself
reaches ~25. `FlameLeviathanCanElectroshock`
([UldBossHelper.cpp:2625](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L2625)) and `SiegeEngineAction`
([UldActions_FlameLeviathan.cpp:199](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L199)) both
make this mistake, so both fire from roughly twice the distance the spell can cover.

### Electroshock: 71 casts, 1 landed, 0 of 40 channels interrupted

Every one of the 40 Flame Vents channels ran its full ~10 s. Three ended early and none of them was
Electroshock: two were `Systems Shutdown` (4 stacks of Overload Circuit, at 1788018794238 and
1788019316922) and one was the boss evading on the wipe.

Only the cast at 1788018247116 produced a `CHRONICLE_SPELL_TARGET_RESULT` (7012 damage). The other 70
were `SPELL_CAST_SUCCESS` with no target — the cone found nothing.

Two separate defects on top of the range mismatch:

- **The election cascades within one channel.** `FlameLeviathanIsVentInterrupter`
  ([UldBossHelper.cpp:2632](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L2632)) picks the highest-energy
  siege engine; casting spends 20, so the next-highest immediately wins and fires ~30 ms later. The
  log shows bursts of 3–4 casts inside 100 ms (e.g. 1788018237041/072/088/110). The comment says
  "one siege engine per channel"; the code delivers four, at 20 energy each.
- **The cooldown is stamped on a cast that never landed.**
  `FlameLeviathanInterruptVentsAction::Execute`
  ([UldActions_FlameLeviathan.cpp:251](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L251))
  calls `AddSpellCooldown(..., 10000)` unconditionally, even though the file's own comment elsewhere
  notes `CastVehicleSpell` returns true when `CheckCast` rejected. A miss locks the engine out for
  10 s, which against the 20 s vent cycle parks it permanently out of phase — the casts cluster in
  the last ~100 ms of a channel or in the gap between channels.

### Battering Ram: the fleet gets cleaved on every target switch

**158 of the 395 Battering Ram hits (40 %) land within 5 s of a Pursued application**, in bursts of
7–13 vehicles at once (13 at 1788018200, 9 at 1788018267, 10 at 1788018740, 12 at 1788018779, 24 at
1788019302, 21 at 1788020329). Pursued rotates every ~31 s.

This confirms the user's read. The mechanism is `HoldStation` →
`DriveTo(FlameLeviathanRearPoint(boss, standDist), boss, false)`, which parks and calls `StopMoving`
([UldActions_FlameLeviathan.cpp:447](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L447)). The
"rear" point is derived from the boss's *current* orientation, so the instant he turns toward a new
Pursued target the parked fleet is in his frontal 25 yd blast. Nothing anticipates the switch and
nothing keeps the vehicles moving with him.

### Pyrite: uptime is the problem, and the stack-level numbers need redoing

Blue Pyrite (68605) is a 10 s DoT, max 10 stacks, 12 120 per stack per tick. Deriving the live stack
count as `tick amount ÷ 12 120` **does not work**, and any figure quoted that way is wrong. The
distinct-amount histogram carries a parallel series at 0.8× — 9696, 19392, 29088, 38784, i.e. 1–4 ×
9696 — so ticks arrive partially reduced, and integer rounding drops a reduced 10-stack tick into the
8- or 9-stack bin. That is the whole explanation for the apparent 10 → 9 → 10 sag between barrels:
the aura refreshes all stacks together and expiry drops all of them at once, so a one-stack dip
cannot happen. Redo this as `(amount + resisted) ÷ 12 120`, or calibrate per caster against that
caster's own maximum observed tick, and re-derive the per-stack distribution before quoting one.

What does **not** depend on tick amounts, and therefore stands:

- **Uptime.** Ticks land once a second, so tick count is DoT uptime in seconds. In attempt 2 (~313 s)
  the five demolishers had 236, 205, 75, 75 and 56 ticks — so one held the DoT ~75 % of the fight and
  three held it for **a fifth of it or less**. Attempts 3 and 4 look the same: 42–126 and 57–154 ticks
  against ~280 s and ~318 s.
- **Cadence.** Median gap between Hurl Pyrite Barrel casts is 4.4 s (p25 3.2 s, p75 5.1 s), from cast
  timestamps alone. `CastVehicle` stamps a 1000 ms cooldown, so ~3.4 s per throw is going somewhere
  else. The observed ramp in one hand-checked window was 1 → 10 stacks over ~30 s.
- **Fuel is not the constraint** — 329 Grab Crate casts in attempt 2, 149 landed refills, 3725 energy.

The likeliest cause of the lost 3.4 s is `CastVehicleSpell` burning a whole tick on
`SetFacingToObject` + `failWithDelay` ([PlayerbotAI.cpp:4127](src/Bot/PlayerbotAI.cpp#L4127)) every
time the moving boss drifts out of a parked demolisher's facing at 65 yd. That needs a trace to
confirm.

**The raw log this analysis came from is gone** — mod-chronicle uploaded and deleted
`instance_603_1_1788031301.log` on the worldserver restart, and the replacement has no FL pull in it
yet. Only the figures extracted above survive. RaidObs traces are not auto-deleted, which is another
reason step 1 comes first.

### Tower hazards were not dodged at all — already fixed outside the code

`FlameLeviathanActiveTowerMask` ([UldHardMode.cpp:61](src/Ai/Raid/Uld/Util/UldHardMode.cpp#L61))
returns `FL_TOWER_ALL` only when `ulduarFlameLeviathanHardMode` is set. During all five logged
attempts the flag was 0, so the mask was 0, `FlameLeviathanDriveUrgentTrigger` never fired for
hazards and `ClearHazard` never ran — while two towers were up. Cost: 9 vehicles and 5.2 M to
Hodir's Fury, 3 vehicles and 3.2 M to Thorim's Hammer.

The user has since set `AC_AI_PLAYERBOT_ULDUAR_FLAME_LEVIATHAN_HARD_MODE=1` and restarted the
worldserver (verified in `/proc/1/environ` of the running process), so this needs no code change —
but it does mean the numbers above are a baseline taken with dodging switched off, and the next pull
is not a like-for-like comparison for the two tower rows.

### RaidObs opens no session for this fight

`boss_flame_leviathan` only ever sets `SPECIAL`, `NOT_STARTED` and `DONE` — never `IN_PROGRESS` — and
the creature it engages is a vehicle, not a roster player. Neither RaidObs opener fires, so no trace
is written even though `AiPlayerbot.Obs.Enabled = 1`. The newest file in `env/dist/logs/botobs/` is
from 2026-08-24 and instance 1 has none at all.

## Plan

Ordered so the instrumentation lands first and the rest can be measured rather than guessed.

### 1. Open a RaidObs trace for Flame Leviathan

Mirror Thorim's corridor
([UldEncounter_Thorim.cpp:454](src/Ai/Raid/Uld/Util/UldEncounter_Thorim.cpp#L454)):
`RaidObs::MarkPull(bot->GetMap(), FlameLeviathanBoss(botAI))`, latched once per instance so it fires
on the first engaged tick and not again. Put the latch beside the existing FL helpers in
`UldBossHelper.cpp` and call it from `FlameLeviathanEngaged` (line 2567), which every FL trigger and
action already routes through. Reset the latch when the boss is gone or out of combat so a re-pull
opens a fresh trace.

Add note probes, following the "probe the rule, not the coordinate" convention in
[docs/systems/observability.md](docs/systems/observability.md):

- `fl.interrupter` — which siege engine the election picked, from inside
  `FlameLeviathanIsVentInterrupter` so trigger and action cannot disagree.
- `fl.station` — which `HoldStation` branch produced the leg (`tar-lead`, `siege`, `demolisher`,
  `chopper`) plus the distance to the boss, via `RaidObs::DescribeDerived`.
- `fl.pyrite` — own Blue Pyrite stack count and remaining duration at the moment `DemolisherAction`
  decides barrel vs boulder.

### 2. Make range tests match the spell, not the hitbox

Replace `IsWithinCombatRange` with a plain distance plus an arc test everywhere an FL cone spell is
gated, since `IsWithinCombatRange` adds the boss's 15 yd reach to every check:

- `FlameLeviathanCanElectroshock` ([UldBossHelper.cpp:2625](src/Ai/Raid/Uld/Util/UldBossHelper.cpp#L2625)):
  `GetExactDist2d(boss) <= ULDUAR_FL_ELECTROSHOCK_CONE_RADIUS` **and**
  `vehicle->HasInArc(M_PI / 2.0f, boss)` — `Spell::SelectImplicitConeTargets` uses a 90° arc, so a
  siege engine that is close but pointed away can never land it.
- `SiegeEngineAction` ([UldActions_FlameLeviathan.cpp:199](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L199)):
  same treatment against `ULDUAR_FL_RAM_CONE_RADIUS`, so 40 energy is not spent on a cone that
  cannot reach.

### 3. Stop parking inside Battering Ram

In `HoldStation` ([UldActions_FlameLeviathan.cpp:350](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L350)):

- Add `ULDUAR_FL_BATTERING_RAM_RADIUS = 25.0f` to `UldBossHelper.h` beside the other FL constants and
  treat "inside 25 yd of the boss's centre and inside his frontal arc" as the thing to stay out of.
- Choppers and demolishers hold station **behind the boss's rear hemisphere**, not merely at his rear
  point, and re-evaluate as he turns rather than parking. `DriveTo`'s `parked_` short-circuit is what
  makes a stale rear point lethal — keep the parked state only while the bot is genuinely behind him.
- Siege engines are the exception: they need 18 yd to Ram. Bring `ULDUAR_FL_SIEGE_STAND_DIST` down so
  `boss->GetCombatReach() + standDist` lands inside the 18 yd Ram cone rather than at 23 yd, and
  accept that they eat Battering Ram — they have 1.13 M HP for it. That single change also fixes the
  199 wasted Ram casts.
- Anticipate the switch: Pursued reapplies on a 31 s cadence (`events.RescheduleEvent(EVENT_PURSUE, 31s)`
  in the core script). Track the last Pursued application per instance in an `ObsValue` so it records
  itself, and from ~5 s before the next expected switch have every non-pursued vehicle clear the
  boss's frontal 25 yd. This is the user's own hypothesis and the 40 % figure above is its evidence.

### 4. Fix the vent interrupt

In `FlameLeviathanInterruptVentsAction::Execute`
([UldActions_FlameLeviathan.cpp:251](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L251)):

- Confirm the cast landed before stamping the cooldown, the way `DemolisherTurretAction` already
  confirms Increased Speed by re-reading the aura — here the honest check is that the boss's
  channelled spell is gone (`!FlameLeviathanIsVentChanneling(boss)`), since Electroshock is instant
  and resolves inline. Only then `AddSpellCooldown(..., 10000)`.
- Latch the elected interrupter per channel so the cascade stops: record the channel's start time (or
  the boss's channel `Spell*` identity) in an instance-scoped `ObsValue` and let
  `FlameLeviathanIsVentInterrupter` return false for everyone once a cast has already gone out for
  that channel. This keeps the "highest energy wins, guid breaks ties" ranking for choosing *who*,
  and adds the missing "only once" for *how many*.

### 5. Make the Hodir's Fury dodge actually shake it

The hard-mode flag is on now, so `ClearHazard` will finally run — but it will still not work for
frost. `GetFlameLeviathanNearestTowerHazard`
([UldHardMode.cpp:66](src/Ai/Raid/Uld/Util/UldHardMode.cpp#L66)) scans for the reticle entries
(33364 / 33369 / 33108) and `ClearHazard`
([UldActions_FlameLeviathan.cpp:339](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp#L339))
flees straight out to `ULDUAR_FL_TOWER_HAZARD_RADIUS + 5`. That is right for Thorim's Hammer —
`_finishTime = 5000 + rand() % 15000` gives 5–20 s of warning on a *static* reticle — but
`NPC_HODIRS_FURY_TARGET` (33108) is a **chaser**: only 2 spawn per attempt and they follow a target,
so a one-shot radial flee is re-entered as soon as it lands.

Treat frost separately: flee perpendicular to the chaser's approach rather than directly away, and
keep re-issuing while it is still closing instead of returning as soon as the 18 yd ring is cleared.
Hodir's Fury does ~79 k a hit against a 509 k chopper, so this is worth getting right even though it
is only 65 hits a fight.

Leave `FlameLeviathanActiveTowerMask` config-driven — the env override is set and verified, and
adding aura-based detection now would be a second source of truth for no gain.

### 6. Pyrite cadence

Land this last — step 1's `fl.pyrite` probe plus the `move` stream should say whether the 3.4 s of
lost time per barrel is the facing round-trip in `CastVehicleSpell` or the drive action winning ticks.
The probe reads `Aura::GetStackAmount` and `GetDuration` off the boss directly, which is the same
value `DemolisherAction` already branches on and settles the stack question without inferring
anything from tick damage.
If it is the facing round-trip, the fix is in `DriveTo`: hold the demolisher's facing on the boss more
aggressively than the current `ULDUAR_FL_FACING_TOLERANCE = 0.1f` drift check while parked at 65 yd,
where a small angular error is a large lateral one. Do not lower `ULDUAR_FL_PYRITE_RESERVE` — the
demolishers were not short of fuel.

## Files

- [src/Ai/Raid/Uld/Util/UldBossHelper.cpp](src/Ai/Raid/Uld/Util/UldBossHelper.cpp) — MarkPull latch,
  `FlameLeviathanCanElectroshock`, `FlameLeviathanIsVentInterrupter`, note probes
- [src/Ai/Raid/Uld/Util/UldBossHelper.h](src/Ai/Raid/Uld/Util/UldBossHelper.h) — stand distances,
  new Battering Ram radius constant
- [src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp](src/Ai/Raid/Uld/Action/UldActions_FlameLeviathan.cpp) —
  `HoldStation`, `DriveTo`, `SiegeEngineAction`, `FlameLeviathanInterruptVentsAction`
- [src/Ai/Raid/Uld/Util/UldHardMode.cpp](src/Ai/Raid/Uld/Util/UldHardMode.cpp) — Hodir's Fury chaser handling
- [docs/raids/ulduar.md](docs/raids/ulduar.md) — record the findings above in the Flame Leviathan section
- [docs/plans/](docs/plans/) — copy this plan to
  `docs/plans/flame-leviathan-vehicle-survival/flame-leviathan-vehicle-survival.PLAN.md`

## Verification

The module cannot be compiled headless here, so verification is a live re-pull, not a build.

1. Hand off for a build, then pull Flame Leviathan with both towers standing again so the comparison
   is like for like.
2. Confirm a trace appears: `env/dist/logs/botobs/603_*_flame-leviathan_*.ndjson`, and
   `.playerbots debug obs` lists it while open.
3. `tools/botobs/postmortem.py <file> --notes fl` — check `fl.interrupter` names exactly one siege
   engine per channel, and `fl.station` shows non-pursued vehicles leaving the frontal arc before each
   Pursued switch.
4. From the chronicle log for the new pull, re-run the same three measurements and compare against
   this baseline:
   - Flame Vents channel durations: currently 40/40 at ~10 s. Any channel under 10 s that is not a
     Systems Shutdown is an Electroshock that landed.
   - Electroshock cast-to-hit: currently 71 casts / 1 `CHRONICLE_SPELL_TARGET_RESULT`.
   - Battering Ram hits within 5 s of a Pursued application: currently 158 of 395 (40 %), in bursts of
     7–13 vehicles.
   - Blue Pyrite DoT uptime per demolisher, as tick count ÷ attempt length: currently 20 %–75 %.
     Derive stacks as `(amount + resisted) ÷ 12 120`, not `amount ÷ 12 120` — the raw amount is
     post-reduction and rounds a full stack into a lower bin.
5. Vehicle survival is the headline number: 60 destroyed across four attempts, none of which killed
   the boss.
