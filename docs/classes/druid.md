# Druid — Feral cat

The only druid work recorded so far. Engine semantics are in
[../engine/action-selection.md](../engine/action-selection.md).

## Faerie Fire (Feral) in the cat rotation

`mod-spell-tweaks` ships `SpellTweaks.OmenClarityFaerieFire.Enable` (default on), which hooks
`AfterHit` on both ranks of Faerie Fire (Feral) — 16857 and 60089 — and **guarantees** a Clearcasting
proc (16870) when the caster has Omen of Clarity (16864) and the target is a PvE creature. FF (Feral)
costs no energy and has a 6s cooldown, so with the tweak on it is a free Shred every 6 seconds.

The rotation was already wired to exploit that (`clearcasting` → `shred` at 24.5, and
`FaerieFireFeralTrigger` has an OoC-aware branch). **The whole problem was arbitration**:
`CatDruidStrategy` registered the action at `5.0f` — an exact tie with the strategy's default `melee`
action (`ACTION_DEFAULT = 5.0f`) and below every cat filler (claw 5.2, mangle 5.3, shred 5.4, pounce
5.5, ravage 5.6), so it could never win selection. Bear registers the same action at 17.0 / 25.5 and
balance registers Faerie Fire at 29.5 — cat was the outlier.

Now at **20.5**: below `cower` (21.0) and all maintenance, well above the filler tier.

Resulting cat ladder: mangle 22.0 → rake 21.5 → cower 21.0 → **faerie fire (feral) 20.5** → fillers
5.6-5.2 → melee 5.0.

## Decisions

- **No new config option.** The trigger keeps gating on the Omen of Clarity aura alone. On servers
  without `mod-spell-tweaks` the branch degrades to chance-based proc fishing costing at most one GCD
  per 6s, and the armor debuff still gets applied — acceptable.
- **A Clearcasting guard was added** so a banked, unspent proc does not cause a wasted GCD:
  Clearcasting is single-charge, so recasting while one is banked buys nothing.
- The bear branch is untouched — bear spams FF for threat regardless of Clearcasting. The non-OoC cat
  fallback (`DebuffTrigger::IsActive()` with `needLifeTime = 8.0f`) is untouched too; it already keeps
  the armor debuff up without spam.
- `CastFaerieFireFeralAction` stays a bare `CastSpellAction`. Its `isPossible()` already runs the
  cooldown check, and **all don't-reapply logic lives in the trigger by design**.
