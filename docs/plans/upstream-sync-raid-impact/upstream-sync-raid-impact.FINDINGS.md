# Upstream sync impact on Naxxramas / Ulduar bot strategies

Review of the AzerothCore boss scripts pulled into `azerothcore-wotlk-pb` by the merges
`aab575e35` (2026-08-02) and `e3c46b79f` (2026-08-08), covering upstream commits from 2026-07-21 to
2026-08-08 under `src/server/scripts/Northrend/{Naxxramas,Ulduar}`.

Scope of the review: does any of it invalidate what `modules/mod-playerbots/src/Ai/Raid/{Naxx,Uld}`
assumes? Five items did; two more opened up behaviour worth using. Everything listed here has been
implemented.

## Breaks

### 1. Razorscale — harpoon chain spells deleted (PR #26602, `3b60a2aa5`)

`SPELL_CHAIN_1..4` (`49679`, `49682`, `49683`, `49684`) no longer exist anywhere in `src/server`.
Harpoons now fire `SPELL_HARPOON_SHOT_1..4` cast by `NPC_RAZORSCALE_CONTROLLER` from
`go_razorscale_harpoon::OnGossipHello`, the harpoon GameObjects are summoned per ground phase by the
controller (two in 10-man, four in 25-man), and a spent harpoon is marked with
`GO_FLAG_NOT_SELECTABLE` until it is rebuilt.

`RazorscaleBossHelper::IsHarpoonFired()` tested `_boss->HasAura(chainSpellId)`, so it silently
returned false for every harpoon.

Fixed by dropping `SPELL_CHAIN_*`, `HarpoonData::chainSpellId` and `IsHarpoonFired()` entirely, and
teaching `IsHarpoonReady()` to reject `GO_FLAG_NOT_SELECTABLE`. Call sites cleaned up in
`UldActions_Razorscale.cpp` and `UldTriggers_Razorscale.cpp`.

### 2. Razorscale — Fuse Armor spell id changed (same PR)

The tank debuff moved from `64771` to `SPELL_FUSE_ARMOR = 64821` (`64774` is still the 5-stack
`Fused Armor`). `64771` is gone from core, so the tank-swap check never fired.
`RazorscaleBossHelper::SPELL_FUSEARMOR` renamed to `SPELL_FUSE_ARMOR` and repointed at `64821`;
`FUSEARMOR_THRESHOLD` (2 stacks) unchanged.

### 3. Thaddius — Stalagg and Feugen feign death (PR #26771, `fcfbe1a4b`)

`DamageTaken` now clamps fatal damage to `health - 1` and puts the add into feign death
(`UNIT_FLAG_NOT_SELECTABLE`, `REACT_PASSIVE`, `UNIT_STAND_STATE_DEAD`, rooted). They only die for
real 12s later, when Thaddius' overload calls `KillSelf()` — 750ms before `EVENT_THADDIUS_INIT` and
about 1.75s before he engages.

`ThaddiusBossHelper` decided the pet phase with `IsAlive()`, so bots stayed parked on the pet
platforms for the whole revive window and had ~1.7s to reach their polarity spots. With a single pet
down, the assigned bots kept attacking an unattackable 1-HP add for the 5s until `ACTION_RESTORE`.

Fixed with a shared predicate — `IsDownOrFeigning()` in `RaidBossHelpers` — applied to
`IsPhasePet`, `GetNearestPet`, `GetPetForSide`, `GetMarkedPet`, `PetSyncSuppress`,
`GetAssignedPetForBot` and both target checks in `NaxxActions_Thaddius.cpp`. `PetSyncSuppress` now
bails out as soon as either pet is down, so the survivor is not throttled against a corpse at 1 HP.

### 4. Vezax — Shadow Crash can target melee (PR #26884, `d07370833`)

The old event only considered players beyond 15 yd; it now picks a random target outside combat
reach and falls back to any target, so the puddle can land in the melee stack or on the tank.

Detection was fine (`VezaxShadowCrashTrigger` keys off the puddle's area aura `63277`), but
`VezaxShadowCrashAction` orbited at a hard-coded 15 yd, which dragged every melee bot out of range
and held it there.

The action now strafes at the bot's own radius: melee and tanks stay in a 4–8 yd band, ranged keep
13–17 yd, and the step is a constant 5 yd of arc so a tight radius still clears the puddle quickly.
Constants live next to the other Vezax tunables in `UldBossHelper.h`.

### 5. Auriaya — Feral Defender feigns death (PR #26762, `c9317a794`)

Each of the defender's nine lives now ends in `SPELL_PERMANENT_FEIGN_DEATH` (58951) rather than a
real death. `GetFirstAliveUnitByEntry` therefore kept returning a feigning defender as the top kill
target. Both call sites use the new `GetFirstLiveUnitByEntry`, which filters on `IsDownOrFeigning`.

## Improvements taken

### 6. Mimiron — Laser Barrage arcs are deterministic (PR #26917, `3da0254d8`)

VX-001 no longer aims the barrage at a random player: arcs start at 6.17 rad, advance +60°
counterclockwise per cast, reset at the start of phases 2 and 4, and the boss faces the arc during
Spinning Up. `spell_mimiron_p3wx2_laser_barrage_aura` then sweeps that facing clockwise at π/60 per
250ms (~12°/s) with the beams following unit facing. Cadence went 45s → 60s.

`MimironP3Wx2LaserBarrageAction` used to teleport/move every bot onto the master. It now reads the
boss facing and moves to `orientation + delta_angle` (π/8 by default) at the bot's own radius,
clamped to 10–24 yd, re-issued each tick so bots trail the beam. Master-follow stays as the fallback
when VX-001 cannot be resolved.

### 7. Yogg-Saron — immunities free a bot from Squeeze (PR #26935, `51fb7d614`)

Removing the `Squeeze` aura (`64125`/`64126`) now kills the Constrictor Tentacle and drops the
passenger. Added `yogg-saron squeeze escape` trigger/action at `ACTION_RAID + 1`: a grabbed mage
casts Ice Block, a paladin casts Divine Shield. Hunter Feign Death and rogue Vanish are deliberately
not used — neither removes a periodic damage aura.

## Reviewed, no change needed

- **Hodir** (#26910, #26911, #26915, #26929, #26930, #26931, `5bdaa898d`): helper spawn positions,
  the Icicle force-cast refactor, toasty fires surviving icicles, the helper spell schedules and the
  Shatter Chest timer aura are all internal. We key off `NPC_SNOWPACKED_ICICLE` / `NPC_TOASTY_FIRE`
  and gate hard mode on `sPlayerbotAIConfig.ulduarHodirHardMode`, and the 3-minute window is
  unchanged.
- **Kologarn** (#26921, #26747): evade despawn and vehicle-accessory arms keep the same entries.
- **Yogg-Saron** (#26968, #26933, #26781, #26713): brain HP sync, the below-platform Constrictor
  filter and reset recovery need nothing from us.
- **Thorim** (#26942, #26746): our gauntlet marking already gates on alive plus RTI state, so the
  new `IMMUNE_TO_PC` staging for the Runic Colossus and Ancient Rune Giant changes nothing.
- **Mimiron** (#26874, #26879, #26841): mine despawn on wipe and magnetic core placement are
  server-side. Rocket Strike (#26844) now spawns `NPC_ROCKET_STRIKE_N` a full 5s before impact, which
  only widens the window our existing dodge already uses.
- **Naxxramas** (#26770): summon despawn on boss death.
- **Halls of Stone** (#24646): 5-man content, no bot strategy exists for it.

## Verification status

Static only — the module cannot be compiled in this environment. Every spell, NPC and GameObject id
touched was cross-checked against the current parent-repo scripts, and the new trigger/action names
were confirmed present in all three wiring sites. A real build and an in-game pass per boss are still
outstanding.
