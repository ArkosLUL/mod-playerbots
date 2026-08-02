# Heroism-before-potion + longer burst dwell

## Context

On a boss pull, shaman bots pop offensive potions **before** Heroism/Bloodlust. Intended order
is lust first (so personal cooldowns + potions land inside the haste window — see the comment on
`LUST_DWELL_MS`). User wants Heroism reliably first, and wants the non-lust burst dwell widened
from 4s to 5s.

### Why it happens (verified)

- Priority is NOT the cause: heroism/bloodlust = `30.0f`, higher than offensive potion
  (`ACTION_HIGH` = `20.0f`) and every shaman nuke (5-6, AoE max 22).
- Cause 1 — narrow dwell gap: heroism gate opens at `LUST_DWELL_MS` = **3000ms**, offensive
  potion at `POTION_HOLD_MS` = **4000ms** (and the burst multiplier's `BURST_DWELL_MS` = 4000ms).
  Only 1s apart.
- Cause 2 — GCD contention: the potion is an item (off-GCD, instant at its gate); heroism is a
  GCD spell, so if the bot is mid-cast when its 3s gate opens it waits for a free GCD and the
  potion beats it out.

Widening `BURST_DWELL_MS` 4→5s pushes the potion's burst gate to 5s while heroism stays at 3s,
giving heroism a 2s head start that survives one in-progress cast.

## Changes (implemented)

### 1. Widen non-lust burst dwell 4s → 5s
`src/Ai/Base/Strategy/BurstWindowStrategy.h:25`
- `BURST_DWELL_MS` `4000` → `5000`. `LUST_DWELL_MS` stays `3000`.
- Affects every non-lust burst cooldown gated by `HoldBurstUntilTankEngagedMultiplier`,
  including offensive potion (via the multiplier, when the `burst` strategy is loaded).

### 2. Match offensive potion's own hold 4s → 5s
`src/Ai/Base/Trigger/GenericTriggers.h:168`
- `POTION_HOLD_MS` `4000` → `5000`. This is the potion's *independent* gate (fires even when the
  `burst` strategy is not loaded), so matching it keeps the potion ~2s behind heroism in every
  case, not just when `burst` is active.

### 3. Bump Heroism/Bloodlust priority 30 → 50
`src/Ai/Class/Shaman/Strategy/GenericShamanStrategy.cpp:136-137`
- Both `NextAction("heroism", 30.0f)` and `NextAction("bloodlust", 30.0f)` → `50.0f`. Clearly
  above rotation (nukes 5-6, AoE max ~22) and below the survival/emergency band, so heroism grabs
  the first free GCD after its gate opens without intruding on emergency actions.

## Verification

- Static: `BURST_DWELL_MS == 5000`, `POTION_HOLD_MS == 5000`, `LUST_DWELL_MS == 3000` unchanged;
  no other reference to the constants outside `BurstWindowStrategy` / `GenericTriggers`.
- In-game (user, needs build): pull a raid boss with a shaman on `burst`; confirm Heroism/Bloodlust
  fires ~3s after the tank engages and the offensive potion follows ~5s, i.e. lust first.
