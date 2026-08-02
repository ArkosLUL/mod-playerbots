# Arms & Fury warrior PvE boss DPS — findings

Why Arms and Fury warrior bots sat at the bottom of the raid DPS meter, what was changed, and what
is still open. Companion to `fury-and-arms-rotation-improvements-plan.md`.

## How the engine picks an ability

`Engine::DoNextAction` ([Engine.cpp:144](../../src/Bot/Engine/Engine.cpp#L144)) pushes every fired
trigger's `NextAction`s plus each strategy's `getDefaultActions()` into a relevance-ordered queue,
pops in descending relevance, and **breaks on the first action that returns `true`**. Constants
([Strategy.h:53-65](../../src/Bot/Engine/Strategy/Strategy.h#L53-L65)): `ACTION_DEFAULT 5`,
`ACTION_NORMAL 10`, `ACTION_HIGH 20`, `ACTION_MOVE 30`, `ACTION_INTERRUPT 40`, `ACTION_EMERGENCY 90`.
`PushDefaultActions` passes `forceRelevance = 0.0f`, so the per-`NextAction` value in
`getDefaultActions()` is kept as-is.

Two trigger base classes matter:

- `BuffTrigger` fires when the named aura is **missing**
  ([GenericTriggers.cpp:193](../../src/Ai/Base/Trigger/GenericTriggers.cpp#L193)). For
  `"bloodthirst"` / `"whirlwind"` there is no such self-aura, so those triggers are permanently
  active — they behave as "always try this".
- `DebuffTrigger` fires when the debuff is missing from the current target
  ([GenericTriggers.cpp:311](../../src/Ai/Base/Trigger/GenericTriggers.cpp#L311)). It cannot drive a
  stack ramp or a cooldown-paced ability, because it goes quiet the moment the aura is present.

## Findings

### Fury — [FuryWarriorStrategy.cpp](../../src/Ai/Class/Warrior/Strategy/FuryWarriorStrategy.cpp)

| # | Problem | Detail |
|---|---------|--------|
| F1 | No Execute phase | No `target critical health` trigger existed, so Execute only competed from `getDefaultActions()` at 5.2 — behind every trigger-driven action. |
| F2 | Heroic Strike dumped rage too early | Fired on `medium rage available` (rage ≥ 40, [GenericTriggers.h:69-73](../../src/Ai/Base/Trigger/GenericTriggers.h#L69-L73)). HS costs 15 (12 talented); at 40 rage it leaves 25, below Bloodthirst's 30. |
| F3 | Death Wish / Recklessness at 20 | Tied with Cleave from the AoE strategy. Both are already gated by `BurstWindowStrategy` ([BurstCooldowns.cpp:22-46](../../src/Ai/Base/Combat/BurstCooldowns.cpp#L22-L46)); only the ordering needed a nudge. |

### Arms — [ArmsWarriorStrategy.cpp](../../src/Ai/Class/Warrior/Strategy/ArmsWarriorStrategy.cpp)

| # | Problem | Detail |
|---|---------|--------|
| A1 | Mortal Strike gated on its own debuff | `MortalStrikeDebuffTrigger` is a `DEBUFF_TRIGGER`. Mortal Wound lasts 10 s, MS cooldown is 6 s → the trigger fired roughly once per 10 s; the rest of the time MS competed from the default list at 5.1. |
| A2 | Bladestorm outranked Mortal Strike | `getDefaultActions()` was bladestorm 5.2 → mortal strike 5.1. Bladestorm is a 6 s channel that locks out MS / Execute / Overpower / Rend. |
| A3 | Slam needed rage ≥ 60 | Under `high rage available`, so the Arms filler almost never fired. Slam is also only correct with Improved Slam talented (or a Bloodsurge proc) — no such check existed. |
| A4 | Execute above Mortal Strike | execute 25 > overpower 24 > mortal strike 23. |
| A5 | `intimidating shout` @ 90 on `critical health` | Fears everything in 8 yd. Bosses are immune, so `CastDebuffSpellAction` never saw the aura land and retried every tick; on trash it broke CC. |
| A6 | `enraged regeneration` @ 90 on `medium health` | Fired at *medium* health, not critical — a GCD plus 15 rage donated to a healer's job. Fury already used `critical health`. |
| A7 | `retaliation` @ 91 on `almost full health` | `CastRetaliationAction::isUseful` needs ≥ 2 melee attackers on the bot ([WarriorActions.cpp:192-236](../../src/Ai/Class/Warrior/WarriorActions.cpp#L192-L236)), so it is inert on a boss but wastes a GCD whenever adds latch on. |
| A8 | `rend on attacker` @ 28 | Redirected Rend onto whatever was hitting the bot instead of the boss. |

### Both specs

| # | Problem | Detail |
|---|---------|--------|
| B1 | Off-GCD abilities consumed a full AI tick | The engine breaks out of `DoNextAction` on the first successful action, then `YieldThread(bot, GetReactDelay())` ([PlayerbotAI.cpp:403](../../src/Bot/PlayerbotAI.cpp#L403)). Heroic Strike, Cleave, Bloodrage, Death Wish, Recklessness and Berserker Rage do not trigger the in-game GCD, but each cost a whole tick — 100-300 ms with a real player master, 500-700 ms for a master-less bot. |
| B2 | AoE strategy hijacked the DPS specs | `"aoe"` is added to both DPS specs unconditionally ([AiFactory.cpp:326-330](../../src/Bot/Factory/AiFactory.cpp#L326-L330)) and `WarrirorAoeStrategy` fired on `light aoe` = 2+ live attackers within 8 yd of the target — true on most boss fights with adds. It pushed `demoralizing shout without life time check` @ 21, plus `thunder clap` @ 25 and `shockwave` @ 24 which DPS specs cannot cast. |
| B3 | No pre-pull Battle Shout | `GenericWarriorNonCombatStrategy` only registered `apply stone` and the fear-break, so every pull opened with a GCD spent on Battle Shout. |
| B4 | Dead stance-prerequisite code | `WarriorStanceRequirementActionNodeFactory` in `GenericWarriorStrategy.h` was never instantiated anywhere — ~200 lines of misleading dead code. |
| B5 | Sunder Armor survived only by accident | `CastSunderArmorAction::isUseful` ([WarriorActions.cpp:67-92](../../src/Ai/Class/Warrior/WarriorActions.cpp#L67-L92)) yields only to a **warrior** tank, so with no prot warrior in the roster the DPS warriors are the raid's Sunder providers. Sunder had no trigger — it sat in `getDefaultActions()` and only landed when nothing else won the tick. |

### Outside the rotation

| # | Problem | Detail |
|---|---------|--------|
| G1 | Weapon-speed preference never reached the runtime gear path | `ApplyPreferredSpecWeapons` returns `1.0f` unless `slot` is MAINHAND/OFFHAND/RANGED. Only `PlayerbotFactory.cpp:2394,2429,2515` passed a slot; every runtime caller used `CalculateItem(id, prop)` with slot defaulting to −1, so the 3× spec-speed bonus shaped initial factory gearing only. Server config was never the issue — `AC_AI_PLAYERBOT_PREFERRED_SPEC_WEAPONS: "1"` is already set. |
| G2 | Arms PvE talent template spends 8 points in Protection | `PremadeSpecLink.1.0.80 = 3022032023335100102012213231251-305-2033` decodes to 55 Arms / 8 Fury / 8 Prot. The Prot points land in Improved Bloodrage / Improved Thunder Clap / Incite — no raid-DPS value. **Decode must be verified in-game before changing.** |
| G3 | Melee hit cap "ignores Precision" — **not a real defect**, see below. |
| G4 | Glyphs unverified + PvP remap hazard | Warrior glyph sets are raw item IDs in conf; unfilled slots get a **random** eligible glyph ([PlayerbotFactory.cpp:4640-4681](../../src/Bot/Factory/PlayerbotFactory.cpp#L4640-L4681)). The PvP remap at `:4404-4416` switches an Arms bot to the arms-**PvP** glyph set purely on the presence of Second Wind (29838). |

Explicitly out of scope: Sunder Armor stays in both DPS default lists (user decision); weapon stones
(`InitConsumables` skipping the ladder above level 75 is intentional); flasks/elixirs and stat food
(handled server-side as world buffs).

## What changed

- **Fury**: added `target critical health` → execute @ 24; Heroic Strike moved to
  `high rage available` (rage ≥ 60) and left at 5.1; Death Wish / Recklessness raised to 21;
  `sunder armor stack` → sunder armor @ 19.
- **Arms**: Mortal Strike now keys off a `CAN_CAST_TRIGGER` (`mortal strike available`) @ 26;
  Bladestorm dropped from the default list and given `bladestorm available` @ 21; Rend lowered to 23;
  Slam moved to `medium rage available` @ 18 and gated on Bloodsurge or Improved Slam; Heroic Strike
  lowered to 5.1; `sunder armor stack` → sunder armor @ 19; `intimidating shout`, `retaliation` and
  `rend on attacker` removed; `enraged regeneration` moved to `critical health`.
- **Sunder Armor (B5)**: new `SunderArmorStackTrigger` in `WarriorTriggers.h/.cpp`, registered as
  `"sunder armor stack"`. It mirrors `CastSunderArmorAction::isUseful` (in a group, no warrior tank,
  debuff below 5 stacks or under 6 s left), so it goes quiet at 5 stacks and costs one refresh per
  ~24 s. `"sunder armor"` still points at the old `DEBUFF_TRIGGER` so `TankWarriorStrategy` is
  untouched.
- **Off-GCD (B1)**: `OffGlobalCooldownAction<Base>` in `WarriorActions.h` wraps Heroic Strike,
  Cleave, Bloodrage, Death Wish, Recklessness and Berserker Rage. It casts, then returns `false` so
  the engine keeps walking the queue and a GCD ability can still land in the same tick.
- **AoE (B2)**: `WarrirorAoeStrategy` now fires on `medium aoe` (3+) and carries only
  sweeping strikes / bladestorm / cleave. Thunder Clap, Shockwave and the un-timed Demoralizing
  Shout moved to a `light aoe` node inside `TankWarriorStrategy`, so protection behaviour is
  unchanged.
- **Battle Shout (B3)**: `battle shout` trigger node added to `GenericWarriorNonCombatStrategy` at
  `ACTION_NORMAL`, reusing `BattleShoutTrigger` (already suppresses itself under a stronger Blessing
  of Might).
- **Dead code (B4)**: `WarriorStanceRequirementActionNodeFactory` deleted.
- **G1**: the equipment slot is now threaded into the runtime scoring calls —
  `ItemUsageValue.cpp` (contested slot in the equip-upgrade loop), `EquipAction.cpp` (mainhand /
  offhand and the ring/trinket pair), `BuyAction.cpp` (candidate vs the item in `dstSlot`). The
  vendor *sort* still scores without a slot, which is deliberate and commented — candidates are
  ranked against each other there, not against an equipped piece.

### Why Step 3 does not use `SetNextCheckDelay(0)`

The plan proposed calling `botAI->SetNextCheckDelay(0)` after a successful off-GCD cast. That cannot
work: `PlayerbotAI::UpdateAI` ends with `YieldThread(bot, GetReactDelay())`, and `YieldThread`
([PlayerbotAIBase.cpp:53-61](../../src/Bot/Engine/PlayerbotAIBase.cpp#L53-L61)) *raises*
`nextAICheckDelay` to the react delay whenever it is lower — so a 0 written from inside an action is
overwritten before the tick ends. Returning `false` from `Execute` is the only warrior-local way to
keep the same tick going, since `DoNextAction` only breaks on `true`.

Two consequences to know about:

- The debug log shows `A:heroic strike - FAILED` for a cast that actually went out. Read the ability
  order from the engine, not from the OK/FAILED tag, for these six abilities.
- Returning `false` makes the engine push the action node's *alternatives*. Arms' `heroic strike`
  node has `melee` as its alternative and its `death wish` node has `bloodrage`; both are harmless
  (they are what the tick would have done anyway, and Bloodrage is itself off-GCD).

## Final relevance tables

Fury (`fury` + `aoe`):

| Relevance | Trigger | Action |
|-----------|---------|--------|
| 40 | pummel / pummel on enemy healer / victory rush | interrupts |
| 39 | enemy out of melee | charge |
| 29 | berserker stance | berserker stance |
| 28 | battle shout | battle shout |
| 27 | bloodthirst (always active) | bloodthirst |
| 26 | whirlwind (always active) | whirlwind |
| 26 | medium aoe | sweeping strikes / bladestorm |
| 25 | instant slam (Bloodsurge) | slam |
| 24 | target critical health | execute |
| 22 | bloodrage | bloodrage *(off-GCD)* |
| 21 | death wish / recklessness | death wish / recklessness *(off-GCD)* |
| 20 | medium aoe | cleave *(off-GCD)* |
| 19 | sunder armor stack | sunder armor |
| 5.5 → 5.0 | *defaults* | bloodthirst, whirlwind, sunder armor, execute, melee |
| 5.1 | high rage available | heroic strike *(off-GCD)* |
| 90 | critical health | enraged regeneration |

Arms (`arms` + `aoe`):

| Relevance | Trigger | Action |
|-----------|---------|--------|
| 41 | shattering throw trigger | shattering throw |
| 40 | victory rush | victory rush |
| 40 | enemy out of melee | charge |
| 30 | battle stance | battle stance |
| 29 | battle shout | battle shout |
| 27 | medium aoe | sweeping strikes |
| 26 | mortal strike available | mortal strike |
| 26 | medium aoe | bladestorm |
| 25 | target critical health / sudden death | execute |
| 24 | overpower / taste for blood | overpower |
| 23 | rend | rend |
| 22 | bloodrage / death wish | bloodrage *(off-GCD)* / death wish *(off-GCD)* |
| 21 | bladestorm available | bladestorm |
| 20 | hamstring | piercing howl |
| 20 | medium aoe | cleave *(off-GCD)* |
| 19 | sunder armor stack | sunder armor |
| 18 | medium rage available | slam |
| 5.1 | high rage available | heroic strike *(off-GCD)* |
| 5.1 → 5.0 | *defaults* | mortal strike, sunder armor, melee |
| 90 | critical health | enraged regeneration |

## Still open

- **G3 is not a defect — no change made.** `ApplyOverflowPenalty`
  ([StatsWeightCalculator.cpp:954-999](../../src/Mgr/Item/StatsWeightCalculator.cpp#L954-L999))
  computes `hit_current` as `GetTotalAuraModifier(SPELL_AURA_MOD_HIT_CHANCE) +
  GetRatingBonusValue(CR_HIT_MELEE)`, which is byte-for-byte what the core itself uses for melee hit
  (`Player::UpdateMeleeHitChances`, `src/server/game/Entities/Unit/StatSystem.cpp:871-875`). Precision
  and every other talent that grants melee hit through that aura is therefore already inside
  `hit_current`, and the 8 % cap is a *total* hit cap, not a gear-only one. Subtracting talented hit
  from the cap on top of that would double-count it and make bots under-value hit by 3 %. If the
  in-game numbers ever disagree, the thing to check is whether Precision's DBC effect really is
  `SPELL_AURA_MOD_HIT_CHANCE` — but if it is not, the bot would not be getting that hit in combat
  either, and the calculator would still be right.
- **G2 (talents) and G4 (glyphs)** need the DB checked first, as the plan says: decode
  `PremadeSpecLink.1.0.80` against a live character before moving the 8 Protection points into Fury,
  and verify the six warrior glyph item IDs before pinning them (Arms: Mortal Strike, Rending,
  Execution; Fury: Whirlwind, Heroic Strike, Execution). The PvP glyph remap at
  `PlayerbotFactory.cpp:4404-4416` also needs a guard so a PvE build carrying Second Wind is not
  reclassified.
- **Bladestorm on cooldown** is now part of the Arms single-target priority (relevance 21). It is a
  6 s channel, so it will occasionally push one Mortal Strike or Execute back. This matches the WotLK
  Arms priority, but it is the first thing to try lowering if Arms still trails.

## Verification

1. **Build** the module — it cannot be compiled headless in this environment.
2. **In-game, `.playerbot` debug**: enable `debug move` plus playerbot action logging on one Arms and
   one Fury bot, pull a Naxx/Ulduar boss with a bot main tank, read the `A:<name>` lines. Expect
   Fury to alternate `bloodthirst` / `whirlwind` with `slam` on every Bloodsurge and `execute` below
   20 %; Arms to land `mortal strike` roughly every cooldown rather than every 10 s, `overpower` on
   every Taste for Blood, and never `intimidating shout` or `retaliation`. Neither spec should cast
   `demoralizing shout` during a 2-add boss phase.
3. **Sunder Armor** is the main regression risk: with no prot warrior in the raid, confirm the boss
   reaches 5 stacks in the opening seconds and never drops below 5 afterwards.
4. **DPS meter**: same boss, same gear level, before/after, against a Retribution Paladin or Frost DK
   bot as the control.
5. **Tank regression**: confirm a Protection warrior's rotation is unchanged — Steps 2, 4 and 6 touch
   shared files, and the prot half of the old AoE node now lives in `TankWarriorStrategy`.
6. **Gear (G1)**: the slot now reaches `ApplyPreferredSpecWeapons` for every class, not just
   warriors. Watch a Hunter, an Enhancement Shaman and a Retribution Paladin bot take a weapon
   upgrade and confirm none of them gets stuck refusing a clear item-level upgrade.
