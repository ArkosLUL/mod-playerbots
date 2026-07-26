# Hunter/Rogue threat redirect: trash misdirection + Naxx boss exclusions

## Context

Hunter bots currently cast Misdirection on the main tank only reactively, from a single trigger
node in `src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp:68`:

```cpp
triggers.push_back(new TriggerNode("low tank threat", { NextAction("misdirection on main tank", 27.0f) }));
```

`LowTankThreatTrigger::IsActive()` (`src/Ai/Base/Trigger/GenericTriggers.cpp:224`) returns true only
when `tankThreat == 0` or the bot already sits above 50% of the tank's threat on its **current
target**. On a trash pack that means the redirect fires once at the pull and then goes quiet, so the
AoE burst that actually generates the pack-wide threat (Volley / Multi-Shot, priorities 22.0 / 21.0)
usually lands with no Misdirection up. Bots rip mobs off the tank.

Two things are wanted:

1. On trash, hunters should proactively keep Misdirection on the main tank and then AoE, so the
   pack's threat lands on the MT.
2. On boss fights the blanket "redirect to MT" is wrong for encounters where the main tank is not
   supposed to own the target the DPS is hitting — Gluth being the obvious one (MT/OT swap on
   Mortal Wound stacks).

Rogue Tricks of the Trade is wired identically (same trigger, same `BuffOnMainTankAction` base) in
`src/Ai/Class/Rogue/Strategy/DpsRogueStrategy.cpp:215` and
`src/Ai/Class/Rogue/Strategy/AssassinationRogueStrategy.cpp:194`, and is in scope for both changes.

Outcome: threat redirect fires on cooldown during multi-mob combat, and is vetoed on the four Naxx
encounters where it breaks the fight.

## Background facts established during exploration

- `MisdirectionOnMainTankTrigger` already exists (`src/Ai/Class/Hunter/HunterTriggers.h:207`) and is
  already registered as trigger `"misdirection on main tank"`
  (`src/Ai/Class/Hunter/HunterAiObjectContext.cpp:90`), but **no strategy uses it**. It is a
  `BuffOnMainTankTrigger` with `checkIsOwner = true`, i.e. it is active exactly when the MT is
  missing *this bot's* Misdirection. Same for the rogue `"tricks of the trade on main tank"` trigger
  (`src/Ai/Class/Rogue/RogueAiObjectContext.cpp:79`).
- `TwoTriggers` (`src/Ai/Base/Trigger/GenericTriggers.h:492`) ANDs two registered triggers by name;
  `getName()` returns `"<name1> and <name2>"`. Existing use: `"medium aoe and healer should attack"`
  in `src/Ai/Base/TriggerContext.h:309`.
- `"light aoe"` = `AoeTrigger(botAI, 2, 8.0f)` — 2+ live attackers within 8 yd of the bot's current
  target; it needs a current target, so it is inherently combat-gated.
- AoE actions already outrank the single-target rotation (Volley 22.0 / Multi-Shot 21.0 vs. 5.x–18.5
  in the spec strategies), and hunters/rogues get the `"aoe"` strategy by default
  (`src/Bot/Factory/AiFactory.cpp:372` and `:376`). **No AoE-side changes are needed** — only the
  redirect timing.
- Per-boss veto via a `Multiplier` returning `0.0f` is the established idiom; `NaxxMultipliers.cpp`
  already does exactly this for `BuffOnMainTankAction` during the Thaddius pet phase
  (`src/Ai/Raid/Naxx/NaxxMultipliers.cpp:172`). The Naxx strategy auto-activates on map id 533
  (`src/Bot/PlayerbotAI.cpp:1671`).
- Boss presence is tested with `AI_VALUE2(Unit*, "find target", "<lowercase creature name>")` — the
  idiom used by every existing Naxx multiplier.
- `BuffOnMainTankAction` is a **shared** base (paladin Beacon of Light / Sacred Shield, shaman Earth
  Shield, druid Thorns / Lifebloom). Vetoes must `dynamic_cast` to the two concrete redirect
  actions, not to the base class.
- Kel'Thuzad already has a dedicated `KelthuzadMisdirectBossToMainTankAction` at `ACTION_RAID + 3`
  (`src/Ai/Raid/Naxx/Action/NaxxActions_Kelthuzad.cpp:412`), which outranks the generic node — leave
  it alone.

## Part 1 — proactive redirect during multi-mob combat

### Hunter

`src/Ai/Class/Hunter/HunterAiObjectContext.cpp` — register a `TwoTriggers` combining the existing
unused trigger with `"light aoe"`, next to the other trigger creators:

```cpp
creators["misdirection on main tank and light aoe"] =
    &HunterTriggerFactoryInternal::misdirection_on_main_tank_and_light_aoe;
...
static Trigger* misdirection_on_main_tank_and_light_aoe(PlayerbotAI* botAI)
{
    return new TwoTriggers(botAI, "misdirection on main tank", "light aoe");
}
```

The creator key must match `TwoTriggers::getName()` output exactly.

`src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp` — add a node next to the existing one in
the Aggro/Threat/Defensive block, keeping `"low tank threat"` for single-target:

```cpp
triggers.push_back(new TriggerNode("misdirection on main tank and light aoe",
                                   { NextAction("misdirection on main tank", 27.0f) }));
```

27.0f already beats Volley (22.0f) and Multi-Shot (21.0f), so the redirect lands before the AoE
burst that feeds it. The `CastBuffSpellAction::isUseful` aura check plus the spell's own cooldown
keep this from spamming.

### Rogue

Mirror in `src/Ai/Class/Rogue/RogueAiObjectContext.cpp`
(`"tricks of the trade on main tank and light aoe"` → `TwoTriggers`), and add the matching
`TriggerNode` at `ACTION_HIGH + 7` in both `DpsRogueStrategy::InitTriggers` and
`AssassinationRogueStrategy::InitTriggers`, alongside their existing `"low tank threat"` nodes.

## Part 2 — Naxxramas boss exclusions

### Verdicts

Blocked — the main tank is not the right threat sink:

| Boss | Why |
|---|---|
| Gluth | MT/OT swap on Mortal Wound stacks; redirect pins the boss on the MT mid-swap |
| Instructor Razuvious | mind-controlled Understudies tank the boss; redirect pulls him onto a player |
| Four Horsemen | four tanks, one horseman each, swaps on Mark stacks |
| Gothik the Harvester | split live/dead sides, no shared main tank (boss strategy is currently commented out, but the veto is still correct) |

Allowed — a single main tank owns whatever DPS is hitting for the whole fight:
Anub'Rekhan, Faerlina, Maexxna, Noth, Heigan, Loatheb, Patchwerk (OT only needs to be *second* on
threat for Hateful Strike, so redirect to MT is the standard opener), Grobbulus, Sapphiron,
Kel'Thuzad (dedicated action already handles it), Thaddius during the Thaddius phase.

Thaddius' pet phase is already covered by the existing `ThaddiusGenericMultiplier` veto.

### Implementation

One new multiplier keeps the boss list in a single place instead of editing four existing
multipliers.

`src/Ai/Raid/Naxx/NaxxMultipliers.h`:

```cpp
class NaxxThreatRedirectMultiplier : public Multiplier
{
public:
    NaxxThreatRedirectMultiplier(PlayerbotAI* ai) : Multiplier(ai, "naxx threat redirect") {}
    float GetValue(Action* action) override;
};
```

`src/Ai/Raid/Naxx/NaxxMultipliers.cpp` (`HunterActions.h` and `RogueActions.h` are already
included):

```cpp
float NaxxThreatRedirectMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<CastMisdirectionOnMainTankAction*>(action) &&
        !dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action))
    {
        return 1.0f;
    }

    // Encounters where the main tank is not the right threat sink: tank swaps on a debuff stack,
    // mind-controlled tanks, or one tank per boss.
    static std::vector<std::string> const noRedirectBosses = {
        "gluth", "instructor razuvious", "gothik the harvester",
        "sir zeliek", "lady blaumeux", "thane korth'azz", "baron rivendare", "highlord mograine"};

    for (std::string const& name : noRedirectBosses)
    {
        if (AI_VALUE2(Unit*, "find target", name))
        {
            return 0.0f;
        }
    }
    return 1.0f;
}
```

All four horsemen are listed, not just `"sir zeliek"`. `FindTargetValue::Calculate` walks
`bot->GetThreatMgr().GetThreatenedByMeList()`, so it only resolves creatures that already have
**this** bot on their threat list — a melee bot parked on Thane or the Baron never sees Zeliek, and
the exclusion would silently not apply to it.

`src/Ai/Raid/Naxx/NaxxStrategy.cpp` — register in `RaidNaxxStrategy::InitMultipliers`:

```cpp
multipliers.push_back(new NaxxThreatRedirectMultiplier(botAI));
```

### Optional cleanup (flagged, not required)

`src/Ai/Raid/Naxx/NaxxMultipliers.cpp:172` vetoes the whole `BuffOnMainTankAction` base during the
Thaddius pet phase, which also suppresses paladin Beacon of Light, shaman Earth Shield and druid
Thorns/Lifebloom on the tank. Narrowing it to the two redirect actions would fix that. Out of the
requested scope — confirm before touching.

## Files touched

- `src/Ai/Class/Hunter/HunterAiObjectContext.cpp`
- `src/Ai/Class/Hunter/Strategy/GenericHunterStrategy.cpp`
- `src/Ai/Class/Rogue/RogueAiObjectContext.cpp`
- `src/Ai/Class/Rogue/Strategy/DpsRogueStrategy.cpp`
- `src/Ai/Class/Rogue/Strategy/AssassinationRogueStrategy.cpp`
- `src/Ai/Raid/Naxx/NaxxMultipliers.h` / `.cpp`
- `src/Ai/Raid/Naxx/NaxxStrategy.cpp`

No new source files, no CMake changes, no new registration sites.

## Verification

There is no unit-test harness for this module and it cannot be compiled headless in the agent
environment, so verification is a build plus in-game observation.

1. **Build**: configure and build AzerothCore with `mod-playerbots` enabled; confirm no new warnings
   in the seven touched files. Watch specifically for the `TwoTriggers` creator keys — a typo there
   fails silently at runtime (trigger not found → node never fires), not at compile time.
2. **Trash (any instance, Naxx is convenient)**: raid with a bot main tank + hunter bot + rogue bot.
   Pull a 3+ mob pack. Expect in the combat log, per pull: `Misdirection` / `Tricks of the Trade` on
   the MT *before* `Volley`/`Multi-Shot`/`Fan of Knives`, repeating as the spell comes off cooldown.
   The MT should keep all mobs.
3. **Gluth (map 533)**: hunters and rogues must cast **no** Misdirection / Tricks for the whole
   fight; MT/OT swap on 5 stacks of Mortal Wound still works.
4. **Instructor Razuvious** and **Four Horsemen**: same — no redirect casts at all.
5. **Patchwerk** (control): redirect still fires normally, confirming the veto is boss-scoped and
   not a blanket kill.
6. **Kel'Thuzad** (regression): the dedicated `kel'thuzad misdirect boss to main tank` behaviour is
   unchanged.
