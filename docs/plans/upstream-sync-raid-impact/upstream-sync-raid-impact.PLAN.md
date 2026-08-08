# Adjust Naxx/Ulduar bot strategies for upstream script changes (Jul 21 – Aug 8 2026)

## Context

The parent repo (`azerothcore-wotlk-pb`) took two upstream syncs — merge `aab575e35` (Aug 2) and
merge `e3c46b79f` (Aug 8) — that reworked several Naxxramas and Ulduar boss scripts. Four of those
changes invalidate assumptions baked into our raid strategies (deleted spell IDs, bosses' adds now
feigning death instead of dying, a mechanic that can now hit melee). Two more changes make
mechanics deterministic or escapable in ways our bots don't yet exploit.

Goal: bring `src/Ai/Raid/Naxx` and `src/Ai/Raid/Uld` back in line with current core behaviour, and
pick up the two new opportunities.

## Confirmed breaks

### 1. Razorscale harpoons (upstream #26602, `3b60a2aa5`)

The rework deleted `SPELL_CHAIN_1..4` (`49679`, `49682`, `49683`, `49684`) — those IDs now appear
**nowhere** in `src/server`. Harpoons fire via `SPELL_HARPOON_SHOT_1..4` (`63658`, `63657`, `63659`,
`63524`) cast by `NPC_RAZORSCALE_CONTROLLER`, triggered from `go_razorscale_harpoon::OnGossipHello`.
Harpoon GOs are now summoned per ground phase by the controller (2 in 10-man: `GO_RAZOR_HARPOON_1/2`;
4 in 25-man) and get `GO_FLAG_NOT_SELECTABLE` set the moment they are used; broken-harpoon GOs
(`194565`) mark not-yet-rebuilt slots.

Our code:
- `RazorscaleBossHelper::SPELL_CHAIN_1..4` and `HarpoonData::chainSpellId` —
  `src/Ai/Raid/Uld/Util/UldBossHelper.h:399-431`
- `IsHarpoonFired()` (`_boss->HasAura(chainSpellId)`) and `GetHarpoonData()` —
  `src/Ai/Raid/Uld/Util/UldBossHelper.cpp:106-172`
- Consumers: `RazorscaleHarpoonAction::Execute` / `::isUseful` —
  `src/Ai/Raid/Uld/Action/UldActions_Razorscale.cpp:543-668`

Fix:
- Drop `SPELL_CHAIN_*` and the `chainSpellId` field; `HarpoonData` becomes just the GO entry list
  (keep all four entries — `HarpoonEntry()` upstream picks a subset per difficulty and
  `FindNearestGameObject` simply misses the absent ones).
- Delete `IsHarpoonFired()` and its two call sites; usability is now purely
  `IsHarpoonReady(harpoonGO)`.
- Extend `IsHarpoonReady()`: reject `harpoonGO->HasGameObjectFlag(GO_FLAG_NOT_SELECTABLE)` on top of
  the existing `GO_STATE_READY` + 5s `_harpoonCooldowns` check. That flag is what upstream now uses
  to mark a spent harpoon, and it is the only reliable "already fired" signal left.
- Keep `SetHarpoonOnCooldown()` — the controller ignores a use while it is already casting, so the
  local cooldown still prevents a bot from burning its turn.

### 2. Thaddius: Stalagg and Feugen feign death (upstream #26771, `fcfbe1a4b`)

`DamageTaken` now clamps fatal damage to `health - 1` and puts the add into feign death
(`UNIT_FLAG_NOT_SELECTABLE`, `REACT_PASSIVE`, `UNIT_STAND_STATE_DEAD`, rooted, `UpdateAI` early-out).
They only really die 12s later, when Thaddius' `reviveTimer` fires `KillSelf()` on them — 750ms
before `EVENT_THADDIUS_INIT` and ~1.75s before he enters combat.

Our code treats `IsAlive()` as "this pet is still up":
- `ThaddiusBossHelper::IsPhasePet()` — `src/Ai/Raid/Naxx/NaxxBossHelper.h:2292`
- `GetNearestPet()` — `:2322-2326`
- `GetPetForSide()` — `:2576-2578`
- `PetSyncSuppress()` — `:2599`
- `GetAssignedPetForBot()` — `:2637-2638`
- `ThaddiusAttackNearestPetAction` target checks —
  `src/Ai/Raid/Naxx/Action/NaxxActions_Thaddius.cpp:32,38`

Effect: phase-pet stays "active" for the whole 12s revive window, so bots start moving to the
Thaddius platform ~1.7s before he pulls instead of ~12s, and land wrong for Polarity Shift. In the
single-pet-down case (5s until `ACTION_RESTORE`) the assigned bots keep trying to attack an
unattackable 1-HP add instead of holding position.

Fix:
- Add a shared predicate in `src/Ai/Raid/RaidBossHelpers.{h,cpp}` next to `GetFirstAliveUnitByEntry`
  (`RaidBossHelpers.cpp:224`), e.g. `bool IsDownOrFeigning(Unit const* unit)` returning true when
  the unit is null, `!IsAlive()`, has `UNIT_FLAG_NOT_SELECTABLE`, or is in
  `UNIT_STAND_STATE_DEAD`. Also add `Unit* GetFirstLiveUnitByEntry(...)` — same body as
  `GetFirstAliveUnitByEntry` but filtered by `!IsDownOrFeigning` — so item 4 can reuse it.
- Replace every `IsAlive()` pet check listed above with `!IsDownOrFeigning(pet)`.
- `PetSyncSuppress()`: bail out (`return false`) when either pet is feigning — the sync window is
  meaningless once one is down, and a feigned pet reads as ~0% health.

### 3. Vezax Shadow Crash can target melee (upstream #26884, `d07370833`)

`EVENT_SPELL_VEZAX_SHADOW_CRASH` used to filter to players >15 yd; it now picks a random target
outside ~3 yd combat reach, falling back to any target. Melee (and the tank) can be hit, so the
puddle can land in the melee stack.

`VezaxShadowCrashTrigger` keys off `bot->HasAura(63277)` — that ID is the *area aura* of the puddle
(`SPELL_VEZAX_SHADOW_CRASH_AREA_AURA` upstream), so detection still works for everyone. The problem
is the reaction: `VezaxShadowCrashAction::Execute`
(`src/Ai/Raid/Uld/Action/UldActions_Vezax.cpp:40-78`) orbits the boss at a hard-coded 15 yd radius,
which yanks every melee bot out of melee range and keeps them there.

Fix — make the action role-aware:
- Melee/tank: step out of the puddle by the shortest route, keeping boss distance — move to the
  bot's current radius (clamped to melee range) rotated by one increment, instead of forcing
  `desiredDistance = 15.0f`. Preserve the existing behaviour for ranged.
- The main tank must not drag Vezax; if `botAI->IsMainTank(bot)`, keep the step small and keep the
  boss within melee reach so the raid's positioning holds.
- Leave `vezax saronite vapors trigger/action` (6 yd, `ACTION_RAID + 1`) untouched — it still owns
  the vapor spawns that follow the crash.

### 4. Auriaya's Feral Defender feigns death (upstream #26762, `c9317a794`)

The defender now takes `SPELL_PERMANENT_FEIGN_DEATH` (`58951`) on each of its nine deaths instead of
actually dying; it revives from the same creature.

Our code resolves it with `GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA_FERAL_DEFENDER)`:
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Auriaya.cpp:66`
- `src/Ai/Raid/Uld/Action/UldActions_Auriaya.cpp:73`

Fix: switch both call sites to the new `GetFirstLiveUnitByEntry` from item 2, so a feigning defender
stops counting as a live target.

## Opportunities

### 5. Mimiron P3Wx2 Laser Barrage is now deterministic (upstream #26917, `3da0254d8`)

VX-001 no longer aims the barrage at a random player. Arcs start at `6.17` rad and advance `+π/3`
counterclockwise per cast, reset at the start of phases 2 and 4, and the boss `SetFacingTo(arc)`
during Spinning Up. During the barrage,
`spell_mimiron_p3wx2_laser_barrage_aura::HandleEffectPeriodic` sweeps the facing **clockwise** at
`π/60` per 250ms (≈12°/s) and the beams follow unit facing. Cadence changed 45s → 60s.

Our reaction is a crutch: `MimironP3Wx2LaserBarrageAction::Execute`
(`src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp:107-119`) teleports/moves every bot onto the master.

Fix — dodge for real, using boss facing (no `AI()->GetData` needed; `SetFacingTo` already exposes
the arc through `GetOrientation()`):
- Each tick while `MimironP3Wx2LaserBarrageTrigger` is active
  (`src/Ai/Raid/Uld/Trigger/UldTriggers_Mimiron.cpp:75-97`, already detects Spinning Up + all three
  barrage spells and the two barrage auras), compute
  `desired = Position::NormalizeOrientation(boss->GetOrientation() + kSafeLag)` with
  `kSafeLag ≈ 0.5` rad. Because the sweep runs clockwise (decreasing orientation), an angle slightly
  *counterclockwise* of the current facing is the spot the beam has just left.
- Move to `boss position + radius * (cos(desired), sin(desired))`, radius = the bot's current
  distance to the boss clamped to a sane band (roughly 10–30 yd), `MovementPriority::MOVEMENT_FORCED`.
  Re-issued each tick this naturally makes bots chase the beam.
- Keep the master-follow branch as the fallback when the boss unit can't be resolved.

### 6. Yogg-Saron: immunities free a bot from Squeeze (upstream #26935, `51fb7d614`)

Removing the `Squeeze` aura (`64125` / `64126`) now kills the Constrictor Tentacle and drops the
passenger. Divine Shield / Ice Block are therefore an escape.

Fix — new trigger + action pair on the Yogg-Saron strategy, following the existing per-boss layout
(`raid-boss-strategy-recipe` skill; wiring sites: `UldTriggerContext.h`, `UldActionContext.h`,
`UldStrategy.cpp`):
- Trigger `yogg saron squeeze escape`: bot has the difficulty-mapped `Squeeze` aura (use
  `sSpellMgr->GetSpellIdForDifficulty`, same as `HodirSpreadStormCloudTrigger`
  `src/Ai/Raid/Uld/Trigger/UldTriggers_Hodir.cpp:84`) and is a paladin or mage.
- Action `yogg saron squeeze escape`: reuse the class-switch immunity pattern from
  `TeronGorefiendAvoidShadowOfDeathAction` (`src/Ai/Raid/BT/BTActions.cpp:640-661`) — mage
  `"ice block"`, paladin `"divine shield"`, guarded by `botAI->CanCastSpell(...)`. Priority
  `ACTION_RAID + 1` so it outranks ordinary rotation but not emergency movement.
- Do not use hunter Feign Death or rogue Vanish here — neither removes a periodic damage aura.

## No action needed (verified)

Hodir helper positions/icicle force-cast/toasty-fire survival/Shatter Chest timer aura (we key off
`NPC_SNOWPACKED_ICICLE` / `NPC_TOASTY_FIRE` and gate hard mode on
`sPlayerbotAIConfig.ulduarHodirHardMode`); Kologarn evade despawn and vehicle-accessory arms; Yogg
brain-HP sync and the below-platform Constrictor filter; Thorim gauntlet immunity staging (our
marking already gates on alive + RTI state); Mimiron proximity-mine despawn and magnetic-core
placement; Naxx summon despawn on boss death. Mimiron Rocket Strike (#26844) now spawns
`NPC_ROCKET_STRIKE_N` a full 5s before impact — strictly more warning for the existing dodge, no
change required.

## Files to modify

- `src/Ai/Raid/RaidBossHelpers.h` / `.cpp` — `IsDownOrFeigning`, `GetFirstLiveUnitByEntry`
- `src/Ai/Raid/Naxx/NaxxBossHelper.h`, `src/Ai/Raid/Naxx/Action/NaxxActions_Thaddius.cpp`
- `src/Ai/Raid/Uld/Util/UldBossHelper.h` / `.cpp` — harpoon data, `IsHarpoonReady`
- `src/Ai/Raid/Uld/Action/UldActions_Razorscale.cpp`
- `src/Ai/Raid/Uld/Action/UldActions_Vezax.cpp`
- `src/Ai/Raid/Uld/Trigger/UldTriggers_Auriaya.cpp`, `src/Ai/Raid/Uld/Action/UldActions_Auriaya.cpp`
- `src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp`
- `src/Ai/Raid/Uld/{Trigger/UldTriggers_YoggSaron,Action/UldActions_YoggSaron}.{h,cpp}`,
  `UldTriggerContext.h`, `UldActionContext.h`, `UldStrategy.cpp` — item 6 wiring
- `docs/raids/ulduar.md`, `docs/raids/naxxramas.md` — update the affected boss sections

## Documentation

Write the analysis up front at
`docs/plans/upstream-sync-raid-impact/upstream-sync-raid-impact.FINDINGS.md`: the commit window, the
six items with upstream commit hashes and PR numbers, and what changed in each of our files. That
doc is the reference for the next sync review.

## Verification

The module cannot be compiled headless in this environment, so verification is static plus a
hand-off:

1. `grep` the tree for the removed IDs (`49679`, `49682`, `49683`, `49684`) and confirm zero hits in
   `modules/mod-playerbots`.
2. Cross-check every spell/NPC/GO constant touched against
   `src/server/scripts/Northrend/Ulduar/Ulduar/*.cpp` and `ulduar.h` in the parent repo.
3. Confirm each new trigger/action name is registered in all wiring sites and that the strings in
   `UldStrategy.cpp` match the context keys exactly (a typo here fails silently at runtime).
4. Hand off for a real build + in-game check, per boss:
   - Razorscale 10 and 25: bots fire every harpoon in each ground phase, no repeated use of a spent
     harpoon, no stalling when only two harpoons exist.
   - Thaddius: after both adds go down, bots leave the pet platforms immediately (not ~12s later)
     and are in position before he pulls.
   - Vezax: melee marked by Shadow Crash step out of the puddle and return; the tank does not drag
     the boss.
   - Auriaya: bots stop attacking the Feral Defender while it is feigning and re-engage on revive.
   - Mimiron P3: bots trail the beam instead of piling on the master.
   - Yogg-Saron: a grabbed paladin/mage pops the immunity and the tentacle dies.
