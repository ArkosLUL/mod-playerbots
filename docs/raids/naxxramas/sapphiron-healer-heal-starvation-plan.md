# Sapphiron — Fix healer air-phase heal-starvation (root cause)

## Context

On Sapphiron (Naxxramas), bot healers stop healing during the air (flight)
phase and the raid wipes. Root cause traced: the flight-positioning action
keeps healers perpetually moving, and the bot engine refuses any cast-time
spell while `bot->isMoving()`. The same jitter also leaves bots mid-move when
Frost Explosion lands, so they aren't behind an ice block (LOS) and die. This
plan fixes the positioning behavior so healers reach shelter, then hold and
heal.

## Root cause (confirmed)

Cast gating: `PlayerbotAI::CanCastSpell/CastSpell` refuse ANY spell with a
non-zero cast time while `bot->isMoving()`
(`src/Bot/PlayerbotAI.cpp:3369-3379`, `:3746-3753`). The `StopMoving()` calls
that would let a bot stop to cast are commented out — movement vs. casting is
resolved purely by action priority. So a bot that keeps receiving move
commands never lands a cast-time heal.

The failure chain in the flight phase:

1. `SapphironFlightPositionAction` is wired at `ACTION_RAID + 1` (61), far above
   the heal bands (light 10 / medium 20 / critical 30,
   `src/Bot/Engine/Strategy/Strategy.h`). While it returns `true` it owns the
   tick and no heal runs.
2. `SapphironGenericMultiplier` whitelists heals during the breath/explosion
   windows (`NaxxMultipliers.cpp:235-258`), so the multiplier is NOT the
   blocker — the physical movement is.
3. `WaitForExplosion()` = flight && any group member has Icebolt
   (`NaxxBossHelper.h:835-841`). Icebolts persist through the air phase, so this
   is true for essentially the whole phase → the position action is active the
   entire flight.
4. `MoveToNearestIcebolt()` (`NaxxActions_Sapphiron.cpp:133-257`) recomputes an
   exact shelter point every tick relative to a moving boss + moving icebolt
   player, and only accepts arrival within `shelterEpsilon = 0.35f`. Tiny drift
   each tick → the bot never settles → chronic micro-movement.
5. The "let the healer heal" escape is broken: `MoveToNearestIcebolt()` returns
   `true` even when it *just issued* a `MOVEMENT_FORCED` move (still moving).
   `Execute` sees `inShelter && IsHeal(bot)` and returns `false` to yield to
   heals — but the bot is moving, so `CanCastSpell` refuses the heal.
   `bot->StopMoving()` is only reached in the exact-arrival branch that jitter
   rarely satisfies.

Net: healers move continuously and cannot cast for the whole air phase (no
healing through Frost Aura + Life Drain + Chill + pre-explosion damage), and
anyone still chasing the exact point when Frost Explosion fires is not behind a
block → dies.

## Fix — reach-then-hold with hysteresis

All changes in `src/Ai/Raid/Naxx/Action/NaxxActions_Sapphiron.cpp`
(`SapphironFlightPositionAction::MoveToNearestIcebolt` and `::Execute`); helper
tweaks in `src/Ai/Raid/Naxx/NaxxBossHelper.h` if state caching is needed.

1. **Acceptance = LOS, not an exact point.** Treat the bot as sheltered when it
   is behind the assigned block on the boss→block line — i.e. the existing
   `playerWithIcebolt->IsInBetween(boss, bot, ...)` LOS test with a widened
   tolerance — instead of requiring `distToLosPos <= 0.35f`. Use a generous
   arrival deadband (e.g. ~3-4 yd) so minor boss/icebolt drift does not
   re-trigger a move.

2. **Hysteresis (reach-then-hold).** Once accepted as sheltered, `StopMoving()`
   and do NOT issue another move until the bot actually loses LOS or drifts
   beyond a larger re-engage radius (> deadband). Track a per-bot "sheltered"
   latch and the assigned block so the target does not hop between blocks each
   tick (stabilize the round-robin assignment for the duration of the phase,
   keeping the existing >20 yd safety fallback to nearest).

3. **Return semantics: own the tick while moving, yield only when stopped.**
   Make `MoveToNearestIcebolt()` (or its caller) distinguish "issued a move"
   (return `true` → position action owns the tick; heals can't fire anyway)
   from "sheltered and stopped" (`StopMoving()` then return so heals run).
   Remove the `inShelter && IsHeal → return false` path that yields while the
   bot is still moving. Non-healers: unchanged — they just need to reach LOS.

Do NOT lower the action priority or widen the multiplier whitelist — the
mechanic must still outrank heals while the bot is genuinely unsheltered; the
fix is to stop issuing moves once LOS is achieved so heals win by default.

## Reuse / references

- Cast-while-moving gate: `src/Bot/PlayerbotAI.cpp:3369-3379`, `:3746-3753`.
- `IsInBetween`, `GetAngle`, `GetDistance2d`, `StopMoving` already used in
  `MoveToNearestIcebolt` — no new primitives needed.
- Priority constants: `src/Bot/Engine/Strategy/Strategy.h`.
- Whitelist untouched: `NaxxMultipliers.cpp:227-278`.

## Verification

Static: confirm `MoveToNearestIcebolt` no longer issues a `MoveTo` on ticks
where the bot already has block LOS within the deadband; confirm `StopMoving()`
is reached on the hold path and the `return false` yield only happens when
stopped.

In-game (Naxx, map 533; strategy auto-activates by map id): pull Sapphiron with
a bot group incl. 2-3 healers. During air phase, confirm healers path to a
block once, stop, and cast heals (watch cast bars / heal log) instead of
sliding in place. Confirm no bot is caught in the open when Frost Explosion
fires (no LOS deaths). Repeat 10- and 25-man and heroic — logic is
difficulty-agnostic (Icebolt id is the only per-difficulty value, already
handled).