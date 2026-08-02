# Resto Shaman healing overhaul — findings

Investigation record for the Restoration Shaman bot healing overhaul. Self-contained: a fresh session can
execute or review the change from this document alone. Paths are relative to `modules/mod-playerbots/`.

## Relevant config defaults (`src/PlayerbotAIConfig.cpp:95-117`)

| Setting | Default |
|---|---|
| `criticalHealth` | 25 |
| `lowHealth` | 45 |
| `mediumHealth` | 65 |
| `almostFullHealth` | 85 |
| `saveManaThreshold` | 60 |
| `healDistance` | 38.5 |
| `sightDistance` | 100 |

`Engine::DoNextAction` picks strictly by relevance across all active trigger nodes, so relevance numbers are
global to the bot's combat engine, not local to a trigger.

## Root cause 1 — the whole heal chain was priced below emergency

`src/Ai/Class/Shaman/Strategy/RestoShamanStrategy.cpp` (before the change) used hand-written literals whose
critical band sat at 23–25. Every other healer opens its critical band at 34–36, because `ACTION_CRITICAL_HEAL`
is 30 (`src/Bot/Engine/Strategy/Strategy.h:53-65`).

Result: `earth shield on main tank` (27), `group heal setting` riptide/chain heal (27/26), `stoneclaw totem`
(40) and `call of the elements` (60) all outranked an emergency heal on a dying player. `water shield` (19.5)
even outranked the low-health riptide (19.0).

## Root cause 2 — Chain Heal was effectively disabled

`chain heal on party` appeared on exactly one trigger node: `group heal setting`, which is
`AoeInGroupTrigger(ai, "group heal setting", "almost full")` (`src/Ai/Base/TriggerContext.h:288`).

`AoeInGroupTrigger::IsActive` (`src/Ai/Base/Trigger/HealthTriggers.cpp:46-62`) bailed on
`GetNearGroupMemberCount() < 5` and then required 3 / 5 / 10 / 15 injured members by group size. In a 25-man
raid that is ten players below 85% HP before a single Chain Heal. Chain Heal appeared in none of the
critical / low / medium / almost-full bands, and Resto Shaman was the only healer spec with no
`medium group heal setting` node at all.

## Root cause 3 — no spell differentiation between bands

All four bands used the identical riptide → healing wave → lesser healing wave chain, differing only by
relevance. Priest, Paladin and Druid each pick fast/cheap spells high in the health range and big heals low
in it.

## Root cause 4 — a mana dead zone where the bot cast nothing

`HealerAutoSaveManaMultiplier::GetValue` (`src/Ai/Base/Strategy/ConserveManaStrategy.cpp:93-130`) zeroes heal
relevance below `saveManaThreshold` (60%) bot mana. The tank branch guards both vetoes with a health check;
the non-tank branch's second `if` had none:

```cpp
else
{
    if (health >= sPlayerbotAIConfig.mediumHealth &&
        (lossAmount < estAmount || manaEfficiency <= HealingManaEfficiency::MEDIUM))
        return 0.0f;
    if (lossAmount < estAmount || manaEfficiency <= HealingManaEfficiency::LOW)   // no health check
        return 0.0f;
}
```

So any `MEDIUM`-or-lower-efficiency heal was vetoed at *any* target health, including 5%. For Resto that
meant: below 60% bot mana, on a non-tank between 50% and 65% HP with riptide already applied,
`lesser healing wave` (LOW), `healing wave` (MEDIUM, estAmount 50) and `riptide` (already on target) were all
vetoed, and the bot fell through to Lightning Bolt.

## Root cause 5 — no healing cooldowns existed

Nature's Swiftness (16188) and Tidal Force (55198) are both Restoration Shaman talents and appeared nowhere in
the Shaman tree. Resto Druid gets `nature's swiftness` at relevance 58; Priest gets Guardian Spirit / Divine
Hymn / Pain Suppression; Paladin gets Lay on Hands / Divine Sacrifice / Divine Illumination. Resto Shaman had
only `mana tide totem` and a self-defence `stoneclaw totem`.

Greater Healing Wave does **not** exist for Shaman in WotLK 3.3.5. The WotLK resto toolkit is Riptide, Chain
Heal, Healing Wave, Lesser Healing Wave, Earth Shield, Earthliving Weapon, Nature's Swiftness, Tidal Force,
Mana Tide Totem, Healing Stream Totem, Water Shield, Cleanse Spirit — nothing was missing from the action
layer except those two cooldowns.

## Relevance table — before / after

`RestoShamanStrategy::InitTriggers`, all values rebased onto the `ACTION_*_HEAL` constants the way
`HealPriestStrategy` and `HealPaladinStrategy` are written.

| Trigger | Before | After |
|---|---|---|
| `party member to heal out of spell range` | reach party member to heal 31 | reach party member to heal 40 (`ACTION_CRITICAL_HEAL + 10`) |
| `enemy too close for spell` | flee 39 | unchanged (`ACTION_MOVE + 9`), now correctly outranked by reach-to-heal |
| `low health` (self) | stoneclaw totem 40 | stoneclaw totem 41 (`ACTION_CRITICAL_HEAL + 11`) — raised only to break the tie with reach-to-heal |
| `party member critical health` | riptide 25, healing wave 24, lesser healing wave 23 | nature's swiftness 58, riptide 36, lesser healing wave 34, healing wave 33, chain heal 32 |
| `nature's swiftness active` | — | healing wave on party 56 |
| `medium group heal setting` | — | tidal force 36.5, chain heal 35, riptide 34.5 |
| `group heal setting` | riptide 27, chain heal 26 | chain heal 28, riptide 27 |
| `medium mana` | mana tide totem 25 (`ACTION_HIGH + 5`) | mana tide totem 26.5 (`ACTION_MEDIUM_HEAL + 6.5`) |
| `earth shield on main tank` | 27 (`ACTION_HIGH + 7`) | 26 (`ACTION_MEDIUM_HEAL + 6`) |
| `party member low health` | riptide 19, healing wave 18, lesser healing wave 17 | riptide 25, chain heal 24.5, healing wave 23.5, lesser healing wave 22.5 |
| `call of the elements` | 60 | 21 (`ACTION_MEDIUM_HEAL + 1`) |
| `party member medium health` | riptide 16, healing wave 15, lesser healing wave 14 | riptide 19, chain heal 18, lesser healing wave 17, healing wave 16 |
| `party member almost full health` | riptide 12, lesser healing wave 11 | riptide 13, chain heal 12 |
| `water shield` | 19.5 | 10.5 (`ACTION_LIGHT_HEAL + 0.5`) — below every heal band |
| dispel nodes | `ACTION_DISPEL + 2` | unchanged |

Half-step values are collision breakers, not design choices. A resto shaman's combat engine also runs
`GenericShamanStrategy` (wind shear 23), `ShamanCureStrategy` (cleanse spirit 24, cleanse on party 23) and the
four totem strategies (`no X totem` re-drops at 55, `set X totem` at 60). The engine breaks relevance ties by
insertion order, so the values that would have collided were nudged: tidal force 36 → 36.5 and medium-group
riptide 34 → 34.5 (against the critical band), mana tide 27 → 26.5 (against group-heal riptide), the low band's
lower three to 24.5 / 23.5 / 22.5 (against the cure and wind shear nodes), and the Nature's Swiftness heal
55 → 56 (against the totem re-drops). Band ordering is unchanged.

Ordering rationale:

- **Riptide leads every band.** Instant, and it grants Tidal Waves, which speeds up the Healing Wave / crits
  the Lesser Healing Wave that follows. The pre-existing riptide-first ordering was accidentally correct.
- **Chain Heal is now in all four bands** — the change that matters most. It is also the only heal that
  survives the save-mana veto in every situation (HIGH efficiency, low estAmount), so it doubles as the
  low-mana backstop.
- **Critical band ends with chain heal, not lesser healing wave**, so the fallback under mana pressure is the
  efficient spell rather than the vetoed one.
- **Medium band puts lesser healing wave above healing wave** — a 2.5s Healing Wave is the wrong tool for
  topping off a 60%-HP target.
- **`call of the elements` drops 60 → 21.** Its trigger fires whenever ≥2 totem slots are empty or out of
  range, so at 60 it re-drops the whole totem set mid-fight while a raider sits at 10% HP. At 21 it still runs
  ahead of routine topping-off but yields to the low and critical bands.

## `estAmount` / `manaEfficiency` tuning

`src/Ai/Class/Shaman/ShamanActions.h`. These two fields are read only by `HealerAutoSaveManaMultiplier`;
`estAmount` is compared against `lossAmount = 100 - targetHealthPct`.

| Action | Before | After | Why |
|---|---|---|---|
| `CastLesserHealingWaveOnPartyAction` | `25.0f, LOW` | `25.0f, MEDIUM` | LOW is vetoed at any health below 60% bot mana on non-tanks. MEDIUM keeps it alive below `mediumHealth`. |
| `CastHealingWaveOnPartyAction` | `50.0f, MEDIUM` | `40.0f, MEDIUM` | estAmount 50 vetoes the cast unless the target is under 50% HP; 40 lets it fire from 60% down. |
| `CastChainHealAction` | `15.0f, HIGH` | `20.0f, HIGH` | Keeps HIGH (the low-mana backstop) while reflecting that a Chain Heal bounce is worth more than a Riptide tick. |
| `CastRiptideOnPartyAction` | `15.0f, VERY_HIGH` | unchanged | Correct as-is. |

## New cooldowns

Mirrors the Druid implementation (`src/Ai/Class/Druid/Action/DruidActions.h:297-301`,
`src/Ai/Class/Druid/DruidTriggers.h:72-77`, `src/Ai/Class/Druid/DruidAiObjectContext.cpp:111-112, 274`).
Spell names resolve against the bot's own known spells, so the Shaman versions (16188 / 55198) are picked up
by name with no ID constants needed; untalented bots fail `CanCastSpell` and the node is skipped.

- `src/Ai/Class/Shaman/ShamanActions.h` — `CastNaturesSwiftnessAction`, `CastTidalForceAction`, both
  `CastBuffSpellAction` subclasses.
- `src/Ai/Class/Shaman/ShamanTriggers.h` — `ShamanNaturesSwiftnessActiveTrigger : HasAuraTrigger`.
- `src/Ai/Class/Shaman/ShamanAiObjectContext.cpp` — trigger `"nature's swiftness active"`, actions
  `"nature's swiftness"` and `"tidal force"`. Registration must be in the **Shaman** context; the Druid
  registration is not visible to Shaman bots, and a missing registration is a silent runtime no-op, not a
  compile error.

## Out-of-combat parity

`src/Ai/Class/Shaman/Strategy/ShamanNonCombatStrategy.cpp` had the same problem in miniature — chain heal only
on `group heal setting`. `chain heal on party` is added to the critical / low / medium / almost-full bands,
one step below riptide in each. The existing OOC relevance ladder (31 → 24) is otherwise untouched; the OOC
engine has no competing high-relevance actions, so no rebase is needed there.

## Shared infrastructure changes and their blast radius

These three affect **all four healer specs**, not just Shaman.

### `AoeInGroupTrigger` thresholds — `src/Ai/Base/Trigger/HealthTriggers.cpp:46-62`

Old: hard bail at `member < 5`, then thresholds 3 / 5 / 10 / 15.
New: bail at `member < 3`, thresholds `≤5 → 3`, `≤10 → 4`, `≤25 → 6`, else `8`.

The group size the thresholds scale from comes from `CountHealableGroupMembers` (alive, inside
`healDistance`), not `GetNearGroupMemberCount` (`sightDistance`, counts the dead). Both sides of the
comparison have to measure the same population or the threshold is unreachable in a spread 25-man.

Blast radius: Chain Heal, Circle of Healing, Prayer of Healing, Wild Growth and Tranquility now fire at
realistic raid damage levels instead of only during a near-wipe.

### `AoeHealValue` counting radius — `src/Ai/Base/Value/AoeHealValues.cpp:12-45`

The counter swept `sightDistance` (100 yd) with no line-of-sight check, so it counted players the bot cannot
actually heal. Restricted to `healDistance` plus `bot->IsWithinLOSInMap(member)`, matching the gate in
`PartyMemberToHeal::Check` (`src/Ai/Base/Value/PartyMemberToHeal.cpp:130-137`).

The value is recalculated on every `Get()`, so the raycast runs last, after the alive / range / health-band
filters — only members that are actually hurt and in range are worth a VMap query.

This reduces counts, which is why it must land together with the threshold change — the two offset each other
and together make the trigger mean "N people I can actually heal are hurt".

### Save-mana health guard — `src/Ai/Base/Strategy/ConserveManaStrategy.cpp:121-128`

Add `health >= sPlayerbotAIConfig.lowHealth &&` to the non-tank branch's second condition, mirroring the tank
branch. Below 45% target HP, mana conservation stops vetoing heals entirely.

Blast radius: fixes root cause 4 and equally benefits Priest (`flash heal`, LOW), Paladin (`holy shock`, LOW)
and Druid (`nourish`, LOW).

## `PartyMemberToProtect` revival (pre-existing bug, unrelated to Resto Shaman)

`PartyMemberToProtect::Calculate` had `return nullptr;` as its first statement
(`src/Ai/Base/Value/PartyMemberToHeal.cpp:165-207`), making the rest of the body unreachable and
`ProtectPartyMemberTrigger::IsActive()` (`src/Ai/Base/Trigger/GenericTriggers.cpp:221`) permanently false.
Three emergency actions have therefore never fired:

- Disc Priest `pain suppression on party` @ 90 (`src/Ai/Class/Priest/Strategy/HealPriestStrategy.cpp:115`)
- Paladin `blessing of protection on party` @ 93 (`src/Ai/Class/Paladin/Strategy/GenericPaladinStrategy.cpp:32`)
- Tank Warrior `intervene` @ 90 (`src/Ai/Class/Warrior/Strategy/TankWarriorStrategy.cpp:324`)

Reviving the value as written would hand a tank below 10% HP to Blessing of Protection — BoP wipes the tank's
melee threat and applies Forbearance, which is worse than the bug. So the revival needs a per-spell target
filter, not just deleting the early return.

`PartyMemberValue` derives from `UnitCalculatedValue`, not `Qualified` (`src/Ai/Base/Value/PartyMemberValue.h:28-42`),
so the filter is exposed as a **second registered value name** rather than a qualifier:

1. `PartyMemberToHeal.h` — `bool excludeTanks` constructor parameter (default `false`) on `PartyMemberToProtect`.
2. `PartyMemberToHeal.cpp` — delete the early `return nullptr;`. Keep the attacker-victim scan and the health
   filter (correct as written: tanks qualify below 10% HP, everyone else below 30%). Add the `excludeTanks`
   skip, plus the inherited `Check(pVictim)` map/distance/LOS/GM gate (`src/Ai/Base/Value/PartyMemberValue.cpp:107-115`)
   and a dead-player skip before pushing onto `needProtect`.
3. `src/Ai/Base/ValueContext.h` — register `"party member to protect no tank"` →
   `new PartyMemberToProtect(botAI, "party member to protect no tank", true)`.
4. `src/Ai/Class/Paladin/Actions/PaladinActions.h` — replace the `PROTECT_ACTION` macro use for
   `CastBlessingOfProtectionProtectAction` with an explicit `CastProtectSpellAction` subclass overriding
   `GetTargetName()` → `"party member to protect no tank"` and `isUseful()` → base check plus
   `!botAI->HasAura("forbearance", GetTarget())`. No context change needed — the factory constructs by class
   name.
5. `src/Ai/Class/Warrior/WarriorActions.h` / `.cpp` — same treatment for `CastInterveneAction`:
   `"party member to protect no tank"` as target, and an `isUseful()` that additionally requires
   `AI_VALUE(uint8, "my attacker count") == 0`. The node lives in `TankWarriorStrategy` at
   `ACTION_EMERGENCY`, so without that guard a tank warrior charges off the boss to any raider below 30%.
6. Priest stays on the generic `"party member to protect"` — Pain Suppression on a dying tank is the intended
   use. `ProtectPartyMemberTrigger` also stays on the generic value, so the Paladin and Warrior nodes still
   fire on the shared trigger and simply report `isUseful() == false` when the candidate is not for them.

Blast radius: Disc Priest, Holy Paladin, Ret Paladin and Protection Warrior all gain an emergency ability they
have never used. Resto Shaman is unaffected — it has no protect node.

## Known follow-ups (deliberately out of scope)

- `PartyMemberToHeal` is uncached: a full group scan with LOS raycasts on every `Get()`. Real perf issue,
  separate change.
- The `AOE_HEAL_ACTION` macro drops its `estAmount` / `manaEfficiency` arguments
  (`src/Bot/Engine/AiObject.h:296-301`), so AoE heal actions built through it get defaults.
- Duplicate `cleanse spirit …` trigger nodes registered by both `RestoShamanStrategy` (52) and
  `ShamanCureStrategy` (23) — harmless, wasted trigger evaluation only.
- Resto's default water totem is Mana Spring rather than Healing Stream (`src/AiFactory.cpp:335-336`).

## Verification

The module cannot be compiled headless in this environment, so verification is static review plus an in-game
pass by the user.

Static:

1. Every action/trigger name string used in `RestoShamanStrategy.cpp` has a matching `creators[...]` entry in
   `ShamanAiObjectContext.cpp`.
2. No two `NextAction` entries reachable in the same tick share a relevance value.
3. `ShamanNaturesSwiftnessActiveTrigger` is registered under `"nature's swiftness active"` in the Shaman
   context.

In-game:

4. Build the module, start a server with a resto shaman bot in the group.
5. `.playerbot debug` on the shaman (enables `Engine::LogAction`) and watch the chosen action per tick while
   damaging a party member through each band: expect riptide → lesser healing wave / chain heal at 60%, and
   riptide → lesser healing wave → healing wave → chain heal at 20%.
6. Drain the shaman below 60% mana and re-check a target at 55% HP — it must still cast (previously it fell
   through to Lightning Bolt).
7. Take a raid group into Naxx 25: Chain Heal should be the dominant cast during raid damage, and Nature's
   Swiftness should fire when someone drops into the critical band.
8. `.playerbot perf` before/after to confirm the `AoeHealValue` radius change did not regress trigger timings.
9. Protect-value regression check with a Disc Priest + Holy Paladin + Prot Warrior group: drop a **non-tank**
   below 30% while a mob is on them — expect Pain Suppression and Blessing of Protection, plus Intervene from
   a warrior that nothing is attacking. Then drop the **tank** below 10% — expect Pain Suppression, and
   explicitly **no** Blessing of Protection and **no** Intervene from the warrior holding the boss.
