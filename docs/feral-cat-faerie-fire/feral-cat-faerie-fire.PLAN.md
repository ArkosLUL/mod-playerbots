# Feral cat DPS: make Faerie Fire (Feral) an actual part of the rotation

## Context

`mod-spell-tweaks` ships `SpellTweaks.OmenClarityFaerieFire.Enable` (default on), implemented at
`modules/mod-spell-tweaks/src/SpellTweaks_classes.cpp:358-398`. It hooks `AfterHit` on both ranks of
Faerie Fire (Feral) (16857, 60089) and **guarantees** a Clearcasting proc (16870) when the caster has
Omen of Clarity (16864) and the target is a PvE creature. FF (Feral) costs no energy and has a 6s
cooldown, so with the tweak on it is a free Shred every 6 seconds.

The playerbot cat rotation is already wired to exploit this — `CatDruidStrategy` has
`clearcasting → shred` at relevance 24.5, and `FaerieFireFeralTrigger` already contains an
OoC-aware branch that returns true every tick for a cat with the talent.

The problem is purely arbitration: `CatDruidStrategy.cpp:273-279` registers the action at **5.0f**,
which is an exact tie with the strategy's default `melee` action (`ACTION_DEFAULT = 5.0f`,
`src/Bot/Engine/Strategy/Strategy.h:55`) and sits below every cat filler (claw 5.2, mangle 5.3,
shred 5.4, pounce 5.5, ravage 5.6). It can therefore never win selection in practice. Bear registers
the same action at 17.0 / 25.5 and balance registers Faerie Fire at 29.5 — cat is the outlier.

Intended outcome: cat bots cast Faerie Fire (Feral) on cooldown to bank a Clearcasting charge (and,
for untalented cats, to keep the armor debuff up), without clipping DoT maintenance, finishers, or
the threat dump.

## Decisions taken

- **No new config option.** The trigger keeps gating on the OoC talent aura alone. On servers
  without `mod-spell-tweaks`, the OoC branch degrades to chance-based proc fishing costing at most
  one GCD per 6s, and the armor debuff still gets applied — acceptable.
- **Relevance 20.5** — below `cower` (21.0) and all maintenance, well above the filler tier.
- **Add a Clearcasting guard** so a banked, unspent proc does not cause a wasted GCD.

## Changes

### 1. `src/Ai/Class/Druid/DruidTriggers.h`

Add a constant next to the existing one at line 20:

```cpp
constexpr uint32 AURA_OMEN_OF_CLARITY = 16864;
constexpr uint32 AURA_CLEARCASTING = 16870;
```

In `FaerieFireFeralTrigger::IsActive()` (lines 123-150), guard the cat/OoC branch. Current code:

```cpp
        // Cat with Omen of Clarity: spam to fish for Clearcasting procs
        if (bot->HasAura(AURA_OMEN_OF_CLARITY))
        {
            Unit* target = GetTarget();
            return target && target->IsAlive() && target->IsInWorld();
        }
```

becomes:

```cpp
        // Cat with Omen of Clarity: cast on cooldown to bank a Clearcasting charge.
        // Clearcasting is single-charge, so recasting while one is banked buys nothing.
        if (bot->HasAura(AURA_OMEN_OF_CLARITY))
        {
            if (bot->HasAura(AURA_CLEARCASTING))
                return false;

            Unit* target = GetTarget();
            return target && target->IsAlive() && target->IsInWorld();
        }
```

Leave the bear branch (lines 128-133) untouched — bear spams for threat regardless of Clearcasting.
Leave the non-OoC cat fallback (`DebuffTrigger::IsActive()`, line 149) untouched — it already keeps
the armor debuff up with `needLifeTime = 8.0f`.

### 2. `src/Ai/Class/Druid/Strategy/CatDruidStrategy.cpp`

Move the `faerie fire (feral)` trigger node out of the filler block at the end of `InitTriggers`
(lines 273-279) and into the maintenance block, immediately after the `medium threat → cower` node
(lines 195-201), with relevance `20.5f`:

```cpp
    triggers.push_back(
        new TriggerNode(
            "faerie fire (feral)", {
                NextAction("faerie fire (feral)", 20.5f)
            }
        )
    );
```

Resulting cat ladder: … mangle (cat) 22.0 → rake 21.5 → cower 21.0 → **faerie fire (feral) 20.5** →
fillers 5.6-5.2 → melee 5.0.

## No changes needed

- `CastFaerieFireFeralAction` (`DruidActions.h:23-27`) stays a bare `CastSpellAction`. Its
  `isPossible()` already runs the cooldown/castability check, and all "don't reapply" logic lives in
  the trigger by design.
- `DruidAiObjectContext.cpp` — trigger and action are already registered (lines 87-88, 147, 201-202,
  299-300).
- `BearDruidStrategy`, `BalanceDruidStrategy`, `DruidPullStrategy` — unaffected.

## Verification

Static:
- Confirm `faerie fire (feral)` appears exactly once in `CatDruidStrategy.cpp` and at 20.5f.
- Confirm no other cat trigger node sits between 20.5 and 21.0.

In-game (requires a compiled server; the module cannot be built headless in this environment — hand
the build off):
1. Ensure `SpellTweaks.Enable = 1` and `SpellTweaks.OmenClarityFaerieFire.Enable = 1`, and that the
   `spell_script_names` rows for 16857/60089 are applied.
2. Spawn a feral-cat bot **with Omen of Clarity talented**, send it at a dummy/creature.
   Expected: FF (Feral) on ~6s cadence, each cast immediately followed by a free Shred (Clearcasting
   consumed at relevance 24.5), and no FF cast while a Clearcasting charge is still unspent.
3. Same bot **without** Omen of Clarity. Expected: FF (Feral) cast once to apply the armor debuff,
   then refreshed only when under 8s remaining — no spam.
4. Bear bot: unchanged behaviour, FF (Feral) still spammed for threat.
5. Check the bot still keeps Rake / Mangle (cat) / Savage Roar / Rip uptime — FF must not be
   displacing them.
