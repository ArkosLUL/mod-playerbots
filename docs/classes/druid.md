# Druid

Feral cat and Balance. Engine semantics are in
[../engine/action-selection.md](../engine/action-selection.md).

## Balance: Moonfire and eclipse order

Ladder (`BalanceDruidStrategy`): Moonfire when missing `ACTION_HIGH + 0.5` → eclipse Starfire/Wrath 20.0 →
Insect Swarm 18.0 → filler Wrath 5.4 over Starfire 5.3. `CastWrathAction`/`CastStarfireAction::isUseful` keep
the eclipse windows: after a Lunar proc Wrath is blocked until Solar procs or 30 s pass, and the reverse.

- **Moonfire sits above the eclipse casts**, as in the sim's APL. The eclipses run back to back, so at 17.5
  Moonfire only landed in the gaps between them: 43-52% uptime against the sim's 97% (TestBotDps).
- **Wrath is the filler, so the bot fishes for Lunar first**, matching the sim (Wrath while Lunar's cooldown is
  ready). With Starfire first, Solar came first and the opener's lust, potion, Hyperspeed and Berserking all
  landed on Wrath, which sits at the 1 s GCD cap and gains nothing from them.
- Insect Swarm is not limited to the post-Lunar Starfire window as the sim's APL does: its hasted ticks
  outdamage the Wrath it replaces.

Open: the potion and Hyperspeed fire when the burst gate opens, not inside Lunar as the sim times them (shared
burst code).

## Feral cat: Faerie Fire (Feral)

`mod-spell-tweaks` ships `SpellTweaks.OmenClarityFaerieFire.Enable` (default on), which hooks
`AfterHit` on both ranks of Faerie Fire (Feral) — 16857 and 60089 — and **guarantees** a Clearcasting
proc (16870) when the caster has Omen of Clarity (16864) and the target is a PvE creature. FF (Feral)
costs no energy and has a 6s cooldown, so with the tweak on it is a free Shred every 6 seconds
(`clearcasting` → `shred` at 24.5).

Cat ladder: mangle 22.0 → rake 21.5 → cower 21.0 → **faerie fire (feral) 20.5** → fillers ravage 5.6 /
pounce 5.5 / shred 5.4 / mangle 5.3 / claw 5.2 → melee 5.0. FF sits below cower and all maintenance but
above the fillers: at `5.0f` it tied `melee` and lost to every filler, so it never won selection. Bear
registers it at 17.0 / 25.5, Balance's Faerie Fire at 29.5.

### Decisions

- **No new config option.** `FaerieFireFeralTrigger` gates on the Omen of Clarity aura alone. On servers
  without `mod-spell-tweaks` the branch degrades to chance-based proc fishing costing at most one GCD
  per 6s, and the armor debuff still gets applied — acceptable.
- **No recast while Clearcasting is banked**: it is single-charge, so the GCD would buy nothing.
- Bear spams FF for threat regardless of Clearcasting. The non-OoC cat fallback
  (`DebuffTrigger::IsActive()`, default `needLifeTime = 8.0f`) keeps the armor debuff up without spam.
- `CastFaerieFireFeralAction` stays a bare `CastSpellAction`. Its `isPossible()` already runs the
  cooldown check, and **all don't-reapply logic lives in the trigger by design**.
