# EoE (Malygos) round 14 — P3 drake healer rotation, Flame Shield, and the DBC numbers behind them

## Context

Test-run-driven iteration on the Eye of Eternity playerbot strategy
(`modules/mod-playerbots/src/Ai/Raid/EoE/`, map 616). Phase 3 puts the whole raid on Wyrmrest
Skytalon drakes with a five-button vehicle bar. Two behaviours are wrong, and both were designed
against guessed spell data.

The real values have now been read out of the client DBC
(`modules/mod-spell-tweaks/data/dbc-reference/spell.reference.csv`, regenerate with
`python tools/dbc_export.py --export-reference`; `SpellRadius.dbc` / `SpellRange.dbc` are not
exported and were parsed straight from `A:\WOW\dbc\Changed`). `acore_world.spell_dbc` is a partial
override table with no rows for these spells, and `spellradius_dbc` / `spellrange_dbc` are empty, so
SQL is not a source for any of this.

| spell | energy | GCD | cooldown | what it actually does |
|---|---|---|---|---|
| Flame Spike 56091 | 10 | 1s | none | 943–1057 damage, +1 combo |
| Engulf in Flames 56092 | 50 | 1s | none | periodic 1500 per 3s, stacks to 999 per caster, combo finisher |
| Revivify 57090 | 10 | 1s | none | ally HoT 500/s for 10s, 5 stacks per caster, +1 combo |
| Life Burst 57143 | 50 | 1s | none | flat 5000 heal in 60y around the caster, self +50% healing done, combo finisher |
| Flame Shield 57108 | 25 | **none** | **none** | −80% damage taken, combo finisher |

Supporting facts, all verified in this repo:

- Skytalon (30161) has 100,000 HP (`creature_classlevelstats` basehp2 12600 × HealthModifier
  7.93651), `unit_class` 4 → energy, cap 100, and `UNIT_FLAG2_REGENERATE_POWER` giving a flat 20
  energy per 2s (`Creature::Regenerate`) = **10 energy/s**.
- No `EffectPointsPerCombo` is non-zero on any of the five, so **combo never scales an amount**. It
  scales duration only: `Unit::CalcSpellDuration` (Unit.cpp:11649) gives
  `min + (max − min) · cp / 5` for any spell whose duration entry has min ≠ max. Flame Shield is
  1s → 6s (1s per combo point), Engulf 2s → 22s, the Life Burst self-buff 0s → 25s.
- Both `SPELL_ATTR1_FINISHING_MOVE_*` bits mean the cast **consumes the whole combo pool**
  (`SpellInfo::NeedsComboPoints`, cleared in `Spell::_handle_finish_phase`). Engulf, Life Burst and
  Flame Shield all carry one. They compete for the same pool — that is the core tension in this
  round.
- Creature casters get **no server-side cooldown at all**: `Spell::SendSpellCooldown`
  (Spell.cpp:4346) returns early for non-players with its `AddSpellCooldown` call commented out.
- P3 Surge of Power 57407: applied 3s after the boss publishes the target guid, then 12,000 arcane
  every 0.5s for 3s = **72,000 on a 100,000 HP drake**. Boss repeats the whole cycle every 7s
  (`boss_malygos.cpp:711`). Malygos is immobile, Arcane Pulse is 28,275 in 30y, a Static Field tick
  is 9,425 in 30y.

### Problem 1 — Flame Shield is built on two false assumptions

`DrakeSurgeShieldAction::Execute` (`EoEActions.cpp:1221`) stamps
`drake->AddSpellCooldown(SPELL_FLAME_SHIELD, 0, 30000)` after a successful cast, and fires the
shield the moment the fixate is seen.

- The 30s cooldown is invented. The spell's real `RecoveryTime`, `CategoryRecoveryTime` and
  `Category` are all 0, and nothing server-side would enforce one anyway. It only ever throws
  shields away.
- Firing at the fixate is the wrong instant. The beam covers **t+3 to t+6**, and the shield lasts
  `1 + cp` seconds. Fired at t+0 holding 0–2 combo points it has expired before the first tick, so
  the drake eats all 72,000. Fired at t+0 holding 5 it works — but it has just burned a bank worth
  a Life Burst or an Engulf on a defensive that only needed 2–3 points to cover the same window.

The rest of the action is sound and stays: the `HasAura` verify is valid because
`Spell::prepare` calls `cast(true)` inline for an instant (Spell.cpp:3691), and returning **false**
so `eoe fly drake` keeps its tick is still right — the shield has no GCD, so it costs the rotation
nothing.

### Problem 2 — healers dump five combo points the moment they have them

`EoEDrakeAttackAction::DrakeHealAction` (`EoEActions.cpp:1185`) casts Revivify on itself until
5 combo points, then fires Life Burst immediately.

Every healer starts the phase together and casts one spell per GCD, so they all hit 5 at the same
second and all burst at once into a full-health flight — 25,000 of overheal, after which the entire
healer corps sits at 0 combo and 0 energy for the next real damage event.

Self-casting Revivify is deliberate and **stays**: combo is held per target
(`Unit::AddComboPoints` clears on a switch), so chasing the lowest drake resets the bank every time
and the finisher never arrives.

What the numbers change is *why* to hold 5. The Life Burst heal is flat 5000 — it is not larger at
5 combo points. What 5 buys is 25s of the self **+50% healing done** buff, which also lifts every
Revivify tick (`SpellPctHealingModsDone`, no creature exclusion). At 50 energy per 25s that is 2 of
the drake's 10 energy/s for a 50% multiplier on everything it does, so the buff should essentially
never be allowed to lapse.

### Not a problem — Engulf in Flames at 3 combo points

Investigated and **no code change**. Engulf is one aura per caster with a 999 stack cap; each
re-application runs `ModStackAmount(1)` → `RefreshTimers`, so the stack shares one duration that
resets on every cast, and the periodic amount is multiplied by the stack
(`SpellAuraEffects.cpp:580`). Stacks therefore grow forever while the aura is alive, and duration's
only job is to outlive the cycle.

The cycle is energy-bound: N spikes plus an Engulf costs `10N + 50` at 10 energy/s = **N+5
seconds**. So 2 combo points would give the most applications per minute (~8.6 vs 7.5, roughly 8%
more damage by 60s), but only 3s of slack between the 10s aura and the 7s cycle. At 3 points the
slack is 6s against a 14s aura, which survives a Static Field dodge or a moment out of range —
and `_TryStackingOrRefreshingExistingAura` never recomputes max duration, so the *first* Engulf of
the phase locks that margin for the whole stack. **3 stays**, for the margin. Going up to 5 would
have been clearly worse (6 applications/min).

## Changes

All paths relative to `modules/mod-playerbots/`.

### 0. Save the plan

Copy this document to `docs/plans/eoe-p3-healer-and-flame-shield/eoe-p3-healer-and-flame-shield.PLAN.md`
before starting.

### 1. Shared drake helpers (`src/Ai/Raid/EoE/EoEActions.h`, `EoEActions.cpp`)

Three small free functions next to the existing `IsDrakeHealer` / `DrakeCanAfford` pair. All of them
walk the raid group and read `member->GetVehicleBase()` filtered on `NPC_WYRMREST_SKYTALON` — the
same iteration `IsDrakeHealer` already does — rather than the creature cache, so they see exactly
the raid's own drakes.

```cpp
// Every Skytalon the raid is currently flying. Walks the group rather than the creature cache
// because a drake is only interesting here if one of ours is riding it.
void GetDrakeFlight(Player* bot, std::vector<Unit*>& drakes);

// Milliseconds left on spellId on this unit, 0 when it is not up. Life Burst's self buff is read
// this way to tell how recently its caster burst.
uint32 DrakeAuraRemainingMs(Unit* drake, uint32 spellId);

// This bot's place in the healer queue: healer drakes ordered by energy descending, guid ascending
// to break ties. Every bot derives the same order from the same observable state, so the flight
// stages its Life Bursts without talking, and bursting drops the caster 50 energy and therefore to
// the back of the queue on its own.
uint8 GetDrakeHealerRank(PlayerbotAI* botAI);
```

Extract the roster walk in `IsDrakeHealer` (`EoEActions.cpp:350`) into a
`GetDrakeHealerGuids(PlayerbotAI*, std::vector<ObjectGuid>&)` so `IsDrakeHealer` and
`GetDrakeHealerRank` share one definition of who is a healer. Keep the guid-sort comment — it is the
reason the whole scheme works without coordination.

New constants in `EoEActions.h` beside `DRAKE_LIFE_BURST_COMBO`:

```cpp
const uint32 DRAKE_LIFE_BURST_BUFF_MS   = 25000;  // full-duration self buff, at five combo points
const uint32 DRAKE_LIFE_BURST_REFRESH_MS = 5000;  // re-burst once the buff is down to this
const uint32 DRAKE_BURST_STAGGER_MS      = 1500;  // hold if another healer burst this recently
const uint8  DRAKE_BURST_HEALTH_PCT      = 90;    // worst drake at or below this wants a burst
const uint8  DRAKE_BURST_EMERGENCY_PCT   = 30;    // ... and at or below this, everyone bursts now
const uint32 DRAKE_LIFE_BURST_HEAL       = 5000;  // flat, does not scale with combo
const uint32 DRAKE_HOLD_ENERGY_FLOOR     = 75;    // Life Burst 50 + Flame Shield 25
```

### 2. Healer rotation (`EoEActions.cpp`, `DrakeHealAction`)

Replace the "5 combo points → burst" branch. Structure:

1. **Below 5 combo points** — Revivify on self if `DrakeCanAfford`, unchanged.
2. **At 5** — decide, in this order:
   - `worstPct <= DRAKE_BURST_EMERGENCY_PCT` → burst now, skip every gate below.
   - Another drake's Life Burst buff has more than `DRAKE_LIFE_BURST_BUFF_MS − DRAKE_BURST_STAGGER_MS`
     left → someone burst within the stagger window; hold.
   - Own buff missing or under `DRAKE_LIFE_BURST_REFRESH_MS` → burst (upkeep).
   - `worstPct <= DRAKE_BURST_HEALTH_PCT` → burst if
     `GetDrakeHealerRank() < ceil(worstMissingHp / DRAKE_LIFE_BURST_HEAL)`, so one healer commits to
     a scratch and more join as it gets worse.
   - Otherwise hold: keep Revivify rolling while energy is at or above `DRAKE_HOLD_ENERGY_FLOOR`
     (the self HoT is worth keeping up and combo is already capped), and idle below it so the bar
     refills for the finisher and the shield. Note Revivify at 10 energy per 1s GCD is exactly the
     10/s regen rate, so without this floor a capped healer never banks the 50 for a burst.
3. **Fixate override** — if this drake is a current Surge of Power target and holds 4 or more combo
   points, burst regardless of the gates above. The points are about to be taken by Flame Shield
   otherwise, and 5000 of raid healing beats nothing.

`DrakeCanAfford` still guards both casts; `CastVehicleSpell` reports success even when the cast it
prepared was rejected, which is the whole reason that helper exists.

### 3. Flame Shield (`EoEActions.cpp`, `DrakeSurgeShieldAction`)

- **Delete** the `HasSpellCooldown` guard and the `AddSpellCooldown(..., 30000)` stamp. Replace the
  guard with `if (drake->HasAura(SPELL_FLAME_SHIELD)) { return false; }` — re-casting would spend
  another bank and re-roll the duration downward.
- **Latch the fixate.** Add `uint32 fixateAtMs` and `uint32 lastSeenMs` members. On each Execute, a
  gap of more than 2s since `lastSeenMs` means this is a new fixate, so restamp `fixateAtMs`. The
  boss clears and refills its guid slots inside one event, so a drake fixated on two consecutive
  cycles can look continuously fixated — it then shields once for both. Rare (one target of N per
  7s cycle) and worth the simplicity; say so in the comment.
- **Time the cast to the beam.** The shield must be up from t+3 to t+6 and lasts `1 + cp` seconds,
  so the latest safe cast is `t + max(0, 5000 − 1000 · cp)` ms, clamped to t+3000 (past that the
  beam is already landing and something beats nothing). Cast when the elapsed time reaches it.
- **Do not eat a full bank.** While `elapsed < 3000`, only shield when combo is 3 or lower; above
  that return false and let the rotation directly below spend the bank on Life Burst or Engulf
  first, then rebuild 2–3 points in the remaining GCDs. At `elapsed >= 3000` shield unconditionally
  — cover matters more than the points at that point.
- Guard with `DrakeCanAfford(drake, SPELL_FLAME_SHIELD)`.
- Keep the `HasAura` verify (it is a genuine post-cast check) and keep returning **false** so
  `eoe fly drake` still gets the tick mid-dodge. Refresh the existing comments: the shield has no
  GCD and no cooldown, so nothing it does costs the rotation a cast.

Energy for the whole fixate script is 50 (finisher) + 20 (two rebuild casts) + 25 (shield) = 95
against +30 of regen across the 3s, so it needs roughly 65 banked. When it is short the affordance
checks simply skip the finisher and the shield still goes up.

### 4. Engulf in Flames

No code change — `DRAKE_ENGULF_COMBO` stays 3. Record the reasoning (stack growth vs. aura margin)
in the doc so the next round does not relitigate it.

### 5. Docs (`docs/raids/eye-of-eternity.md`)

Reference documentation, so ordinary prose conventions apply. Rewrite the P3 drake bullets to carry
the real numbers rather than the guesses they currently describe:

- The drake spellbook table above, with the point that combo scales duration and never amount, and
  that all three finishers share one pool.
- The healer bullet: why Revivify stays self-cast, why 5 combo points is about buff uptime rather
  than heal size, the burst gates and the energy-rank stagger, and the `DRAKE_HOLD_ENERGY_FLOOR`
  reason (Revivify is exactly break-even against regen).
- The Flame Shield bullet: replace the invented-cooldown passage entirely — real cooldown 0, no GCD,
  −80%, `1 + cp` seconds, the t+3..t+6 beam window and the cast-time formula, and why it yields to
  the rotation while holding 4 or more points.
- The Engulf bullet: 3 combo points, and that it is a margin choice, not a damage-maximising one.

## Verification

Static first — the module cannot be compiled headless here, so the build and the in-game checks are
a hand-off.

1. **Static**: no wiring changes — `EoEStrategy.cpp`, `EoEMultipliers.*`, `EoEActionContext.h`,
   `EoETriggerContext.h`, `RaidStrategyContext.h`, `BuildSharedActionContexts.cpp`,
   `BuildSharedTriggerContexts.cpp` and `PlayerbotAI.cpp` are all untouched;
   `EoETriggers.cpp`/`.h` only if `DrakeSurgeTrigger` needs exposing for the fixate override.
   Check brace balance, `awk 'length>120'` on the sources (only the pre-existing
   `EoEActions.h:315` `AvoidSurgeOfPowerAction` ctor should flag) and `length>104` on the doc, and
   grep that no reference to the removed 30s cooldown survives.
2. **Flame Shield, several surges**: pick a fixated drake. It must not shield at the instant of the
   fixate while holding a full bank — it should spend the bank on Life Burst or Engulf, rebuild, and
   put the shield up about a second before the beam. Damage taken from Surge of Power should land
   near 14,000 instead of 72,000. A drake that fails one attempt must retry within the window rather
   than go quiet.
3. **Healers, light damage**: on a full-health flight, healers must reach 5 combo points and
   **hold**. Exactly one burst at a time, roughly one per healer per 25s for buff upkeep — never all
   of them in the same second.
4. **Healers, real damage**: after an Arcane Pulse or a Static Field, the number of healers that
   burst should scale with how hurt the worst drake is, and a drake dropping under 30% should pull
   every healer's burst at once.
5. **Energy**: no healer should ever sit at 5 combo points with a full bar, and none should be
   caught at 0 energy when a fixate lands.
6. **Regression**: drake dps unchanged — Flame Spike ×3 then Engulf, and the Engulf stack on Malygos
   should keep climbing rather than falling off between casts (`.debug` or a combat log check on
   56092 stack count).
