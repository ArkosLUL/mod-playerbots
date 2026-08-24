# Freya (Ulduar) — Detonating Lasher corral

## Context

In-game testing of a `GROUP_LASHERS` wave: ten Detonating Lashers scatter across the raid, each
chasing a random player, and their on-death Detonate lands wherever they happen to be. Two requests
came out of it — drag a lasher that has picked a non-tank over to the off-tank, and root the pile
with Frost Nova once six or more are parked; a hunter Frost Trap was added as a third tool.

Intended outcome: a lasher wave is fought at one spot behind Freya instead of inside the raid, held
there by a snare patch and a periodic root, and killed by ranged standing outside Detonate's 15 yd.

**One request cannot be honoured as written and the design below replaces it.** No tank can hold a
Detonating Lasher. `boss_freya.cpp:1256-1262`: every 10 s each one casts Flame Lash, then
`DoResetThreatList()` and `AttackStart(SelectTargetFromPlayerList(80))`, which is a **uniformly random
alive player within 80 yd** (`ScriptedCreature.cpp:591-609`). A taunt survives one tick at most.
`GetFreyaTankTarget` and `docs/raids/ulduar.md:601` already say so. Threat is therefore not the
mechanism; geometry, a snare and a root are. The off-tank still parks at the corral and taunts, as
chosen, but it is worth roughly one parked lasher, not ten.

## Ground truth (verified — do not re-derive)

### Detonating Lasher (32918)

| Fact | Value | Source |
|---|---|---|
| Retarget | every 10 s: Flame Lash on victim, `DoResetThreatList()`, then a uniform random alive player within 80 yd | `boss_freya.cpp:1256-1262` |
| Despawn | if no player is within 80 yd it despawns and still counts as cleared | same |
| Wave size | **10** per `GROUP_LASHERS` wave; waves are exclusive (trio / Conservator / lashers), so a lasher wave has **no Conservator and no spore** | `boss_freya.cpp:1229-1234` |
| Spawn | 5 s submerged and passive, then picks its first target | `boss_freya.cpp:1101-1115` |
| Speed | `speed_run` 1.14286 → **8.0 yd/s**, faster than a player's 7.0 — it follows a dragger and is never dropped | `creature_template` |
| Flame Lash 62608 | weapon damage +150, melee range — cheap to carry | DBC |
| Detonate 62598 | on **death only** (`JustDied`), 4163-4837 fire, **15 yd** (radius idx 18), no difficulty entry | `boss_freya.cpp:1174-1175`, DBC |
| Immunities | `flags_extra = 0`, `rank = 1`; this fork's `creature_template` has no `mechanic_immune_mask` column and the script sets none — root and snare both land | `acore_world` |

### Frost Nova (122 … 42917)

- **10 yd PBAoE centred on the caster** — `ImplicitTargetA = 22` (TARGET_SRC_CASTER), `TargetB = 15`,
  `RangeIndex = 1`, `EffectRadiusIndex = 13` (10 yd). The mage has to stand in the pack, which is
  inside Detonate's 15 yd.
- Root: `EffectAura_2 = 26` (MOD_ROOT), `EffectMechanic_2 = 7` (MECHANIC_ROOT), duration idx 31 =
  **8000 ms**. `CategoryRecoveryTime = 25000` → **25 s cooldown**.
- **Does not break on damage.** `AuraInterruptFlags = 0x480000` is TELEPORTED | CHANGE_MAP only, and
  `Unit::DealDamage` (`Unit.cpp:1032`) removes break-on-damage auras solely via
  `AURA_INTERRUPT_FLAG_TAKE_DAMAGE` (0x2). Entangling Roots is byte-identical.
- **No diminishing returns here.** Frost Nova maps to `DIMINISHING_CONTROLLED_ROOT`
  (`SpellMgr.cpp:149-151`), whose type is `DRTYPE_PLAYER`, and `Unit::ApplyDiminishingToDuration`
  only diminishes a creature carrying `CREATURE_FLAG_EXTRA_ALL_DIMINISH`. The lasher has
  `flags_extra = 0`. The 10 s PvP duration cap is gated the same way and also does not apply.

So the root is a full, repeatable 8 s through raid AoE — better than expected.

**This contradicts `UldBossHelper.h:1416`**, which claims "Entangling Roots and Frost Nova break on
the first hit". Neither does in 3.3.5. The Mimiron Bomb Bot conclusion still stands (Frost Nova is
self-centred and useless against something approaching at range), but the stated reason is wrong and
is corrected as part of this work.

### Frost Trap (13809) — the best of the three

- Places a trap at the hunter's feet (`RangeIndex = 1`), **30 s shared trap cooldown**
  (`CategoryRecoveryTime = 30000`, category 411), patch lasts **30 s** (duration idx 9). Cooldown
  equals duration, so one hunter sustains it.
- Frost Trap Aura 13810: `Mechanic = 11` (SNARE), `EffectAura_1 = 33` (MOD_DECREASE_SPEED),
  `EffectBasePoints_1 = -51` → **-50 % movement speed**, `EffectRadiusIndex = 13` (**10 yd**),
  re-applied every 2000 ms. No damage break — it is a snare, not a CC. A lasher leaving the corral
  moves at 4.0 yd/s instead of 8.0.
- `botAI->CastSpell("frost trap", bot)` is already the established call in this codebase
  (`NaxxActions_Gluth.cpp:213`, `ICCActions_LK.cpp:4221`, `UldActions_Kologarn.cpp:163`).
- **Freezing Trap is not usable here**: Freezing Trap Effect 3355 has `AuraInterruptFlags = 0x480002`
  — the 0x2 bit is TAKE_DAMAGE, so raid damage breaks it instantly. Freezing Arrow (60192, 40 yd)
  only places that same breakable trap remotely.

### Corral geometry (navprobe-verified, map 603)

Freya spawns at `(2338.46, -52.3255, 425.552)`, orientation `3.1765`. She never moves, but she turns
to face whoever tanks her, so the direction must come from `Creature::GetHomePosition()`, not the
live orientation.

Corral = home position + **35 yd** along `homeOrientation + π`.

- `ring 2338.46 -52.3255 425.552 35 16` → **16/16 on mesh**, settledZ within 0.4-3.0 yd of the floor
  on every heading. The same ring at 50 yd has four southern headings falling back to the input Z, so
  35 is the outer clean radius.
- The computed point `(2373.42, -51.10)` probes on-mesh: nearest poly 0.978 yd, terrain Z 424.247,
  `UpdateAllowedPositionZ` rewrite of -1.3 yd (normal — `CheckCollisionAndGetValidCoords` handles it).

The raid enters from in front of Freya and the main tank holds her there, so "behind her" is always
away from the raid.

## Settled decisions (do not relitigate)

- Off-tank parks at the corral and taunts, accepting that it holds roughly one lasher at a time.
- Corral is deterministic: behind Freya, from her **home** orientation, 35 yd. No shared state, so
  every bot computes the same spot and none of them oscillate.
- **Only ranged and healers drag.** A melee bot that ferried a lasher would end up standing in the
  corral, which is exactly where the Detonate chain goes off. Melee keep the existing 12 yd leash.
- A mage novas, then steps out past 16 yd.
- A dragger below `ULDUAR_FREYA_DETONATE_FLEE_HEALTH` abandons the trip **except** within 12 yd of the
  corral, where it commits and finishes.

## Why the corral does not become a 45 k bomb

Ten Detonates in one pile would be ~45 k inside 15 yd. It stays survivable because
`GetFreyaRangedLasherFocus` already makes every ranged bot focus **one lasher at a time** (lowest
health, GUID breaking ties), so deaths are staggered rather than simultaneous. The off-tank standing
at the corral is the one bot exposed to the chain, and it eats them one at a time. Raid AoE bringing
several low at once is the residual risk — call it out in the docs, do not try to gate AoE for it.

---

## 1. `Ai/Raid/Uld/Util/UldBossHelper.{h,cpp}`

New constants, beside the existing lasher block:

```cpp
// 35 yd behind Freya, measured from her home orientation because she turns to face her tank. The
// whole 35 yd ring around her spawn probes on-mesh; at 50 yd the southern headings stop settling.
constexpr float ULDUAR_FREYA_LASHER_CORRAL_DISTANCE = 35.0f;
constexpr float ULDUAR_FREYA_LASHER_CORRAL_ARRIVE = 5.0f;   // close enough to have delivered
constexpr float ULDUAR_FREYA_LASHER_CORRAL_COMMIT = 12.0f;  // inside this a hurt dragger finishes
constexpr float ULDUAR_FREYA_LASHER_PACK_CLEAR = 16.0f;     // one yard past Detonate

// Frost Nova is a 10 yd sphere on the caster, so this is also how close the mage has to stand.
constexpr float ULDUAR_FREYA_FROST_NOVA_RADIUS = 10.0f;
constexpr uint32 ULDUAR_FREYA_LASHER_PACK_MIN_COUNT = 6;

// The trap patch is 10 yd around the hunter's feet, so posting it one Detonate radius short of the
// corral covers the lane back to the raid while keeping the hunter out of the blast.
constexpr float ULDUAR_FREYA_LASHER_TRAP_OFFSET = 16.0f;
```

New helpers:

```cpp
// Behind Freya, from her home orientation - she turns to face her tank, so the live one drifts.
// Returns Position() before she is found.
Position GetFreyaLasherCorral(PlayerbotAI* botAI);
// On the line from the corral back to Freya, ULDUAR_FREYA_LASHER_TRAP_OFFSET short of the corral.
Position GetFreyaLasherTrapPost(PlayerbotAI* botAI);
uint32 CountFreyaLashersWithin(Player* bot, FreyaWaveState const& state, float radius);
// The live lasher currently chasing this bot, or nullptr.
Unit* GetFreyaLasherChasing(Player* bot, FreyaWaveState const& state);
```

`GetFreyaLasherCorral` runs the result through `bot->GetMap()->CheckCollisionAndGetValidCoords(...)`.

Also fix the wrong root claim at `UldBossHelper.h:1416` — neither Entangling Roots nor Frost Nova
breaks on damage in 3.3.5; the reason Frost Nova is absent from the Bomb Bot list is that it is
self-centred and cannot reach something still approaching.

## 2. `Ai/Raid/Uld/Trigger/UldTriggers_Freya.{h,cpp}`

Four new triggers, all requiring Freya alive:

- **`freya drag lasher to corral`** — `PlayerbotAI::IsRanged(bot) || botAI->IsHeal(bot)`, not a tank,
  `GetFreyaLasherChasing` returns something, and the bot is farther than
  `ULDUAR_FREYA_LASHER_CORRAL_ARRIVE` from the corral.
- **`freya lasher pack step out`** — ranged or healer, not a tank, and
  `CountFreyaLashersWithin(bot, state, ULDUAR_FREYA_DETONATE_RADIUS) >= ULDUAR_FREYA_LASHER_PACK_MIN_COUNT`.
  This doubles as the return leg of the drag — no separate walk-back node.
- **`freya frost nova lashers`** — `CLASS_MAGE`, count within `ULDUAR_FREYA_FROST_NOVA_RADIUS` meets
  the minimum, and `botAI->CanCastSpell("frost nova", bot)`.
- **`freya trap lasher corral`** — `CLASS_HUNTER`, the designated hunter (lowest GUID among alive
  hunter bots in the group, the same tie-break `GetFreyaRangedLasherFocus` uses), lashers are up, and
  `botAI->CanCastSpell("frost trap", bot)` or the bot is not yet on its post.

One change to an existing trigger: **`FreyaAvoidDetonatingLasherTrigger` returns false within
`ULDUAR_FREYA_LASHER_CORRAL_COMMIT` of the corral**, so a nearly-arrived dragger is not pulled off the
trip by the 5500 HP step-out.

## 3. `Ai/Raid/Uld/Action/UldActions_Freya.{h,cpp}`

- **`FreyaDragLasherToCorralAction`** — `MoveTo(corral, …, MOVEMENT_FORCED, true, false)`. Nothing
  else; the lasher is faster than the bot and follows on its own.
- **`FreyaLasherPackStepOutAction`** — gathers every live lasher within
  `ULDUAR_FREYA_HAZARD_SEARCH_RADIUS` and calls `FindNearestPositionClearOfHazards(bot, blasts,
  ULDUAR_FREYA_LASHER_PACK_CLEAR, ULDUAR_FREYA_HAZARD_SEARCH_RADIUS)`, exactly as
  `FreyaAvoidDetonatingLasherAction` already does.
- **`FreyaFrostNovaLashersAction`** — `botAI->CastSpell("frost nova", bot)`. Cast directly rather than
  through `DoSpecificAction("frost nova")`: `CastFrostNovaAction::isUseful` gates on the **current
  target** being within 10 yd, and a ranged mage's current target is the focused lasher, usually far.
- **`FreyaTrapLasherCorralAction`** — walk to `GetFreyaLasherTrapPost` if farther than
  `ULDUAR_FREYA_LASHER_CORRAL_ARRIVE`, otherwise `botAI->CastSpell("frost trap", bot)`.
- **`FreyaTankAddsAction`** gains a corral branch: when assist tank 0's ladder has nothing (no
  Snaplasher, no Conservator, no trio member alive) and lashers are up, walk to the corral, then
  `UldCastClassTaunt` the nearest lasher not already attacking this bot. Add `challenging shout` /
  `challenging roar` when the count at the corral meets the minimum — both are registered
  (`TankWarriorStrategy.cpp:254`, `BearDruidStrategy.cpp:138`), both are 3 min, and neither is used
  anywhere else on this encounter. Paladin `righteous defense` is deliberately skipped: it taunts
  attackers of a *friendly* target, not an area.

## 4. Registration and wiring

`UldTriggerContext.h` and `UldActionContext.h` get the four new names. `UldStrategy.cpp` gets four
trigger nodes; priorities are chosen so the sequence resolves without a multiplier:

| Node | Priority | Beats |
|---|---|---|
| `freya frost nova lashers` | `ACTION_RAID + 4` | the step-out — nova first, leave second |
| `freya lasher pack step out` | `ACTION_RAID + 3` | the drag — an arrived dragger leaves the pile |
| `freya avoid detonating lasher` | `ACTION_RAID + 3` (unchanged) | — |
| `freya drag lasher to corral` | `ACTION_RAID + 2` | — |
| `freya trap lasher corral` | `ACTION_RAID + 2` | — |

No CMake change: every file already exists. No multiplier change — `FreyaDisableAutomaticTargeting`
already zeroes generic assist for tanks and DPS across the whole encounter.

## 5. Docs

`docs/raids/ulduar.md`, Freya section: the corral, who drags and why melee do not, the three
retarget/snare/root numbers above, the staggered-death argument for why the corral is survivable, and
the corrected note that roots do not break on damage in 3.3.5. Correct `UldBossHelper.h:1416` in the
same pass.

`compact-governing-docs` applies — `docs/raids/ulduar.md` is reachable from the module `CLAUDE.md`.
**Invoke `/compact-docs-writer` fresh before touching it.**

On approval this plan is saved to
`modules/mod-playerbots/docs/plans/freya-lasher-corral/freya-lasher-corral.PLAN.md`.

## Deliberately out of scope

- Any attempt to hold lashers by threat beyond the off-tank's single-target taunt.
- Freezing Trap / Freezing Arrow (breaks on damage).
- Melee repositioning during a lasher wave — the existing 12 yd leash stands.
- Gating raid AoE to keep lasher deaths staggered.

## Verification

The module cannot be compiled headless here, so the build is a hand-off.

1. Static checks: the four names appear in exactly three places each (context, strategy, class);
   `ULDUAR_FREYA_LASHER_CORRAL_DISTANCE` is used only through `GetFreyaLasherCorral`; the corral
   helper reads `GetHomePosition()`, never `GetOrientation()`.
2. Build the worldserver with the module.
3. **Lasher wave, 25 m.** Ranged and healers that get picked walk behind Freya and come back;
   melee never leave. The pack visibly gathers ~35 yd behind her.
4. Confirm the off-tank is standing at the corral and that its taunt lands — and re-rolls off within
   10 s, which is expected, not a bug.
5. Confirm a hunter posts ~19 yd behind Freya and that Frost Trap is down continuously (30 s patch,
   30 s cooldown). Lashers crossing it should visibly halve speed.
6. Confirm a mage novas once six are inside 10 yd, that the root holds a full 8 s **through raid
   damage**, and that the mage then steps past 16 yd.
7. Confirm no bot below 5500 HP is stuck ferrying — except inside 12 yd of the corral, where it
   should finish the trip.
8. Watch raid damage taken across a lasher wave against a pre-change run; Detonate hits on the raid
   should drop sharply, and the wave should still die inside the 60 s clock.
9. Repeat in 10 m and with `UlduarFreyaHardMode = true`.
