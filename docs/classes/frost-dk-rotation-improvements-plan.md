# Frost DK bot uses tank rotation instead of frost DPS

## Context

A level-70 Frost DK bot with full frost talents (incl. Howling Blast and Frost Strike), dual-wielding
one-handers, casts Death Strike instead of the frost rotation, parks itself in Frost Presence, and
pulls aggro. The user's `co ?` listing showed `frost` + `frost aoe` but also `tank assist`,
`tank face`, `pull`, `pull back` — the tank support set.

Two independent defects produce this:

**1. DK spec-tab detection defaults to blood.** `AiFactory::GetPlayerSpecTab`
([AiFactory.cpp:68-107](modules/mod-playerbots/src/Bot/Factory/AiFactory.cpp#L68-L107)) returns the
fallback tab whenever `level < 10` or the talent-point sum is 0 (which also happens when
`GetActiveSpecMask()` doesn't match the stored `PlayerTalent::specMask`, e.g. after a talent reset or
dual-spec switch). The fallback `switch` at lines 90-104 handles mage/paladin/priest/warlock only —
DK falls through to `tab = 0` = `DEATH_KNIGHT_TAB_BLOOD`. `AddDefaultCombatStrategies`
([AiFactory.cpp:391-398](modules/mod-playerbots/src/Bot/Factory/AiFactory.cpp#L391-L398)) then applies
`"blood", "tank assist", "pull", "pull back"`. That is exactly the residue the user saw.

Death Strike has only two emitters, both in `BloodDKStrategy`
([lines 146](modules/mod-playerbots/src/Ai/Class/Dk/Strategy/BloodDKStrategy.cpp#L146),
[178](modules/mod-playerbots/src/Ai/Class/Dk/Strategy/BloodDKStrategy.cpp#L178)) at relevance 23 and
21 — an order of magnitude above `FrostDKStrategy`'s default actions (5.0-5.7), so blood dominates any
tick it is active. `FrostDKStrategy` never emits Death Strike.

Manually adding `frost` evicts `blood` (sibling context,
[Engine.cpp:359-374](modules/mod-playerbots/src/Bot/Engine/Engine.cpp#L359-L374)) but leaves
`tank assist` / `pull` / `pull back` / `tank face` behind — they aren't siblings — so the bot keeps
tanking. `ResetStrategies` clears everything first
([PlayerbotAI.cpp:1887-1899](modules/mod-playerbots/src/Bot/PlayerbotAI.cpp#L1887-L1899)), so any
reset re-derives the wrong set from the same bad tab.

**2. Frost Presence makes any DK a tank.** `PlayerbotAI::IsTank(player, /*bySpec*/true)`
([PlayerbotAI.cpp:2275](modules/mod-playerbots/src/Bot/PlayerbotAI.cpp#L2275)) returns true for
`tab == BLOOD || player->HasAura(SPELL_DK_FROST_PRESENCE)`. `BloodDKStrategy`'s action nodes carry
prerequisite `frost presence` ([lines 30-77](modules/mod-playerbots/src/Ai/Class/Dk/Strategy/BloodDKStrategy.cpp#L30-L77)),
so blood force-casts Frost Presence, which then classifies the bot as a tank regardless of talents —
self-reinforcing. That drives `tank face` ([AiFactory.cpp:401](modules/mod-playerbots/src/Bot/Factory/AiFactory.cpp#L401))
and group tank-role logic ([PlayerbotAI.cpp:2099](modules/mod-playerbots/src/Bot/PlayerbotAI.cpp#L2099),
[RandomPlayerbotMgr.cpp:2798](modules/mod-playerbots/src/Bot/RandomPlayerbotMgr.cpp#L2798)).

Separately, `FrostDKStrategy`'s rotation is thin even when it does run: Howling Blast fires only on a
Rime proc or 3+ AoE targets, Frost Strike has no runic-power trigger, Obliterate has no rune trigger,
and `KillingMachineTrigger` / `HowlingBlastTrigger` are declared but never registered.

## Changes

### 1. Presence no longer implies tank — `src/Bot/PlayerbotAI.cpp`

Drop the `player->HasAura(SPELL_DK_FROST_PRESENCE)` clause at line 2275 so the DK case reads
`tab == DEATH_KNIGHT_TAB_BLOOD` only, matching how warrior/paladin are handled. Blood is
unambiguously the tank tree — unlike druid feral, there is no shared-tree case needing an aura probe.
Remove the now-unused `SPELL_DK_FROST_PRESENCE` constant at
[PlayerbotAI.cpp:62](modules/mod-playerbots/src/Bot/PlayerbotAI.cpp#L62) if nothing else references it.

Blast radius is small: only `bySpec = true` callers reach this branch. The `bySpec = false` default
(used by gear weighting via `ItemUsageValue`/`StatsWeightCalculator`) resolves through
`ContainsStrategy(STRATEGY_TYPE_TANK)` and is unaffected. Fix the stale comment at
[ItemUsageValue.cpp:676-679](modules/mod-playerbots/src/Ai/Base/Value/ItemUsageValue.cpp#L676-L679),
which claims `IsTank` keys off frost presence.

### 2. DK spec-tab fallback — `src/Bot/Factory/AiFactory.cpp`

Add `case CLASS_DEATH_KNIGHT: tab = DEATH_KNIGHT_TAB_FROST; break;` to the fallback switch at
lines 90-104, so a DK with no countable talents defaults to DPS rather than tank. Consistent with the
existing mage/paladin/priest/warlock entries, which all default to a DPS or healing tree.

Also fix the bogus initializer at
[AiFactory.cpp:112](modules/mod-playerbots/src/Bot/Factory/AiFactory.cpp#L112) —
`std::map<uint8, uint32> tabs = {{0, 0}, {0, 0}, {0, 0}}` builds a one-entry map because all three keys
are `0`. Should be `{{0, 0}, {1, 0}, {2, 0}}`. Behaviour is unchanged (`operator[]` value-initializes),
but it is misleading.

### 3. Frost rotation — `src/Ai/Class/Dk/`

**New trigger** in `DKTriggers.h` / `DKTriggers.cpp`:

```cpp
class HighRunicPowerTrigger : public Trigger
{
public:
    HighRunicPowerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "high runic power") {}
    bool IsActive() override;   // bot->GetPower(POWER_RUNIC_POWER) >= 800  (units are power*10)
};
```

Verify the power scale against an existing `GetPower` call before picking the literal; WotLK runic
power is stored ×10, so 80 RP is `800`. Model `IsActive` on the existing rune triggers at
[DKTriggers.cpp:42-65](modules/mod-playerbots/src/Ai/Class/Dk/DKTriggers.cpp#L42-L65).

**Register in `DKAiObjectContext.cpp`** (`DeathKnightTriggerFactoryInternal`, lines 58-93):

- `creators["killing machine"]` → `new KillingMachineTrigger(botAI)` — the class already exists at
  [DKTriggers.h:124-128](modules/mod-playerbots/src/Ai/Class/Dk/DKTriggers.h#L124-L128) but is
  unreachable. Note: this is the *trigger* namespace; the same-named *action* node in
  `GenericDKStrategy` is separate and stays as-is.
- `creators["high runic power"]` → the new trigger.
- `creators["frost and unholy runes"]` → `new TwoTriggers(botAI, "high frost rune", "high unholy rune")`.
  Both component triggers are already registered (lines 84-85); `TwoTriggers` is already used this way
  at [DKAiObjectContext.cpp:101-104](modules/mod-playerbots/src/Ai/Class/Dk/DKAiObjectContext.cpp#L101-L104).
  Obliterate costs one frost + one unholy rune, so both must be up.

**Delete `HowlingBlastTrigger`** ([DKTriggers.h:106-110](modules/mod-playerbots/src/Ai/Class/Dk/DKTriggers.h#L106-L110)).
Unregistered dead code, and wrong as written — a `DebuffTrigger` keyed on a debuff named
"howling blast", which does not exist.

**New trigger nodes** in `FrostDKStrategy::InitTriggers`
([FrostDKStrategy.cpp:100-157](modules/mod-playerbots/src/Ai/Class/Dk/Strategy/FrostDKStrategy.cpp#L100-L157)).
Everything here sits **below** the existing disease triggers at `ACTION_HIGH + 2` (22): Howling Blast
does not apply Frost Fever without Glyph of Howling Blast, so Icy Touch and Plague Strike must always
get first refusal. Ordering is glyph-independent — if the bot does happen to carry the glyph, diseases
are already up from Icy Touch and nothing is lost.

| Trigger | Action | Relevance | Why |
|---|---|---|---|
| `freezing fog` (existing node, re-priced) | `howling blast` | `ACTION_HIGH + 1` (21) | Rime proc makes HB rune-free; consume it before it expires. Currently 5.5, so today it loses to Obliterate at 5.7. |
| `killing machine` | `frost strike` | `ACTION_HIGH` (20) | Guaranteed-crit proc, spent on a runic-power dump so no rune is wasted. |
| `high runic power` | `frost strike` | `ACTION_NORMAL + 9` (19) | Stops RP capping. |
| `frost and unholy runes` | `obliterate` | `ACTION_NORMAL + 8` (18) | Main rune spender, fired when the runes are actually up. |

Keep the existing default actions ([lines 90-98](modules/mod-playerbots/src/Ai/Class/Dk/Strategy/FrostDKStrategy.cpp#L90-L98))
unchanged as the fallback ladder — Obliterate 5.7 / Frost Strike 5.4 / Horn of Winter 5.1 / melee 5.0.

**Check, don't change:** `AiPlayerbot.PremadeSpecGlyph.6.1` (frost PvE) is
`45805,43673,43547,43544,43672,43543`
([playerbots.conf.dist:1868](modules/mod-playerbots/conf/playerbots.conf.dist#L1868)). Item `43544`
falls in the DK glyph ID range and may be Glyph of Howling Blast — resolve it against `item_template`
and report. If it is, the template is spending a major slot on a glyph the user doesn't want; that's a
config decision, not a code change, and the priorities above work either way.

Rune gating matters here because `PlayerbotAI::CanCastSpell` builds its probe with
`TRIGGERED_IGNORE_POWER_AND_REAGENT_COST`
([PlayerbotAI.cpp:3412-3415](modules/mod-playerbots/src/Bot/PlayerbotAI.cpp#L3412-L3415)), so
Obliterate reports castable with no runes available, wins the tick at 5.7, fails, and burns a queue
iteration before Frost Strike gets a turn.

## Out of scope (flagged, not changed)

- Blood as a DPS tree: `"blood"` maps unconditionally to `BloodDKStrategy` (`STRATEGY_TYPE_TANK`)
  and `"tank"` is an alias for it ([DKAiObjectContext.cpp:42-43](modules/mod-playerbots/src/Ai/Class/Dk/DKAiObjectContext.cpp#L42-L43)).
  A blood-spec DPS DK is not representable.
- `CastKillingMachineAction` and the `"killing machine"` / `"improved icy talons"` action nodes cast
  passive proc auras and can never succeed. Left alone — nothing emits them.
- `FrostDKAoeStrategy::InitTriggers` doesn't chain to `CombatStrategy::InitTriggers`; matches
  `UnholyDKAoeStrategy`, so left consistent.

## Verification

The module can't be compiled headless in this environment, so this is a static-review + in-game
hand-off. Build the module into the AzerothCore server, then in-game:

1. `.playerbot bot add <DKname>`, then `/w <DKname> reset ai` to force `ResetStrategies`.
2. `/w <DKname> co ?` — expect `frost`, `frost aoe`, `dps assist` and **no** `tank assist`,
   `tank face`, `pull`, `pull back`.
3. `/w <DKname> nc ?` — expect `dps assist`, not `tank assist` / `pull`.
4. Pull a single target with a real tank in group. Confirm in the combat log: no Death Strike, no
   Frost Presence cast; Icy Touch + Plague Strike open **before** any Howling Blast, Obliterate as the
   rune spender, Frost Strike dumping runic power, Howling Blast on Rime procs, Blood Presence held.
   Watch specifically that Frost Fever and Blood Plague never drop while HB is being cast.
5. Pull 3+ mobs. Confirm Howling Blast and Death and Decay fire.
6. Confirm the bot no longer takes aggro off the tank.
7. Regression: spawn a blood DK bot, `reset ai`, confirm it still gets `blood` + `tank assist` +
   `pull` + `pull back` + `tank face`, holds Frost Presence, and uses Death Strike / Rune Strike.
8. Regression: spawn a fresh level-55 DK with zero talents; confirm it now gets the frost DPS set
   rather than the tank set.

Enable `AiPlayerbot.LogInGroupOnly = 0` and check the bot's action log (`S:+frost`, `PUSH:obliterate`,
`A:frost strike - OK`) if any step disagrees.
