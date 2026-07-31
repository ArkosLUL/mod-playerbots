# Fix Arms & Fury Warrior PvE boss DPS

## Context

Arms and Fury warrior bots consistently sit at the bottom of the DPS meter on raid boss
fights. Investigation of `src/Ai/Class/Warrior/` plus the surrounding engine, gear and
consumable code found the cause is not one bug but a stack of them: broken ability
priorities, missing Execute-phase handling, defensive/utility abilities firing at
emergency priority inside boss fights, off-GCD abilities burning a whole AI tick, an AoE
strategy that hijacks the rotation on any 2-mob pull, and out-of-rotation losses
(weapon-speed preference never reaching the runtime gear path, wasted talent points).

Goal: bring both specs' boss-fight throughput in line with the other melee DPS bots
without touching tank warriors.

### How the engine picks an ability (needed to read the rest of this)

`Engine::DoNextAction` ([Engine.cpp:144](modules/mod-playerbots/src/Bot/Engine/Engine.cpp#L144))
pushes every fired trigger's `NextAction`s plus each strategy's `getDefaultActions()` into a
relevance-ordered queue, pops in descending relevance, and **breaks on the first action that
returns `true`**. Constants ([Strategy.h:55-65](modules/mod-playerbots/src/Bot/Engine/Strategy/Strategy.h#L55-L65)):
`ACTION_DEFAULT 5`, `ACTION_HIGH 20`, `ACTION_MOVE 30`, `ACTION_INTERRUPT 40`, `ACTION_EMERGENCY 90`.
`PushDefaultActions` passes `forceRelevance = 0.0f`, so the per-`NextAction` value in
`getDefaultActions()` is kept as-is.

Two trigger base classes matter:
- `BuffTrigger` fires when the named aura is **missing** ([GenericTriggers.cpp:193](modules/mod-playerbots/src/Ai/Base/Trigger/GenericTriggers.cpp#L193)).
  For `"bloodthirst"` / `"whirlwind"` there is no such self-aura, so those triggers are
  permanently active — they behave as "always try this".
- `DebuffTrigger` fires when the debuff is missing from the current target
  ([GenericTriggers.cpp:311](modules/mod-playerbots/src/Ai/Base/Trigger/GenericTriggers.cpp#L311)).
  Used for `"mortal strike"`, which is why MS's high-priority trigger only fires once per
  debuff duration, not once per cooldown.

---

## Findings

### Fury — [FuryWarriorStrategy.cpp](modules/mod-playerbots/src/Ai/Class/Warrior/Strategy/FuryWarriorStrategy.cpp)

Effective priority today: bloodthirst 27 > whirlwind 26 > slam(Bloodsurge) 25 > bloodrage 22 >
death wish / recklessness 20 > *defaults*: bloodthirst 5.5, whirlwind 5.4, sunder armor 5.3,
execute 5.2, heroic strike 5.1, melee 5.0.

| # | Problem | Detail |
|---|---|---|
| F1 | **No Execute phase at all** | There is no `target critical health` trigger for Fury (Arms has one), so Execute only ever competes from `getDefaultActions()` at 5.2 — i.e. after every trigger-driven action in the queue. Sub-20% it should be a top-priority ability. |
| F2 | **Heroic Strike dumps rage too early** | Trigger is `medium rage available` = rage ≥ 40 ([GenericTriggers.h:69-73](modules/mod-playerbots/src/Ai/Base/Trigger/GenericTriggers.h#L69-L73)). HS costs 15 (12 talented); firing at 40 leaves 25, below Bloodthirst's 30, so BT gets delayed by rage. Should dump at ≥ 60. |
| F3 | **Death Wish / Recklessness at 20** | Same relevance as Cleave from the AoE strategy; both are already correctly gated by `BurstWindowStrategy` ([BurstCooldowns.cpp:22-46](modules/mod-playerbots/src/Ai/Base/Combat/BurstCooldowns.cpp#L22-L46)), so only the ordering needs a nudge above filler. |

### Arms — [ArmsWarriorStrategy.cpp](modules/mod-playerbots/src/Ai/Class/Warrior/Strategy/ArmsWarriorStrategy.cpp)

| # | Problem | Detail |
|---|---|---|
| A1 | **Mortal Strike gated on its own debuff** | `MortalStrikeDebuffTrigger` is a `DEBUFF_TRIGGER` ([WarriorTriggers.h:27](modules/mod-playerbots/src/Ai/Class/Warrior/WarriorTriggers.h#L27)). Mortal Wound lasts 10 s, MS cooldown is 6 s → the 23-relevance trigger fires roughly once per 10 s. The rest of the time MS only competes from the default list at 5.1, where anything else in the queue beats it. |
| A2 | **Bladestorm outranks Mortal Strike** | `getDefaultActions()` is bladestorm 5.2 → mortal strike 5.1. Bladestorm is a 6 s channel that locks out MS / Execute / Overpower / Rend. |
| A3 | **Slam needs rage ≥ 60** | Under `high rage available`, so the Arms filler almost never fires. Slam is also only correct with Improved Slam talented (otherwise it is a 1.5 s cast that clips auto-attacks) — no talent check exists. |
| A4 | **Execute above Mortal Strike** | execute 25 > overpower 24 > mortal strike 23. Sub-20% MS should lead. |
| A5 | **`intimidating shout` @ 90 on `critical health`** | Fears everything in 8 yd. Bosses are immune, so `CastDebuffSpellAction` never sees the aura land and retries every tick while the bot is low; on trash it breaks CC and scatters mobs. |
| A6 | **`enraged regeneration` @ 90 on `medium health`** | Fires at *medium* health, not critical — a GCD + 15 rage donated to a healer's job. Fury correctly uses `critical health` for the same action. |
| A7 | **`retaliation` @ 91 on `almost full health`** | `CastRetaliationAction::isUseful` needs ≥ 2 melee attackers on the bot ([WarriorActions.cpp:173-217](modules/mod-playerbots/src/Ai/Class/Warrior/WarriorActions.cpp#L173-L217)) so it is inert on a boss, but wastes a GCD whenever adds latch on. |
| A8 | **`rend on attacker` @ 28** | Redirects Rend onto whatever is hitting the bot instead of the boss. |

### Both specs

| # | Problem | Detail |
|---|---|---|
| B1 | **Off-GCD abilities consume a full AI tick** | The engine breaks out of `DoNextAction` on the first successful action, then `YieldThread(bot, GetReactDelay())` ([PlayerbotAI.cpp:403](modules/mod-playerbots/src/Bot/PlayerbotAI.cpp#L403)). Heroic Strike, Cleave, Bloodrage, Death Wish, Recklessness and Berserker Rage do not trigger the GCD in-game, but here each one costs a whole tick — 100-300 ms with a real player master, 500-700 ms for a master-less bot ([GetReactDelay, PlayerbotAI.cpp:6634](modules/mod-playerbots/src/Bot/PlayerbotAI.cpp#L6634)). Warriors are hit hardest because HS/Cleave are meant to go out nearly every swing. |
| B2 | **AoE strategy hijacks DPS specs** | `"aoe"` is added to both DPS specs unconditionally ([AiFactory.cpp:327-330](modules/mod-playerbots/src/Bot/Factory/AiFactory.cpp#L327-L330)) and `WarrirorAoeStrategy` ([GenericWarriorStrategy.cpp:59-72](modules/mod-playerbots/src/Ai/Class/Warrior/Strategy/GenericWarriorStrategy.cpp#L59-L72)) fires on `light aoe` = 2+ live attackers within 8 yd of the target ([GenericTriggers.h:309-313](modules/mod-playerbots/src/Ai/Base/Trigger/GenericTriggers.h#L309-L313)) — true on most boss fights with adds. It pushes `demoralizing shout without life time check` @ 21, a tank debuff that eats a GCD every time it drops, plus `thunder clap` @ 25 and `shockwave` @ 24 which DPS specs can't cast (wrong stance / prot talent) and which only burn CPU. |
| B3 | **No pre-pull Battle Shout** | `GenericWarriorNonCombatStrategy` ([:33-40](modules/mod-playerbots/src/Ai/Class/Warrior/Strategy/GenericWarriorNonCombatStrategy.cpp#L33-L40)) only registers `apply stone` and the fear-break. Every pull opens with a GCD spent on Battle Shout, and the raid is unbuffed for the first seconds. |
| B4 | **Dead stance-prerequisite code** | `WarriorStanceRequirementActionNodeFactory` ([GenericWarriorStrategy.h:15-217](modules/mod-playerbots/src/Ai/Class/Warrior/Strategy/GenericWarriorStrategy.h#L15-L217)) is never instantiated anywhere. ~200 lines of misleading dead code. |
| B5 | **Sunder Armor survives only by accident** | `CastSunderArmorAction::isUseful` ([WarriorActions.cpp:48-73](modules/mod-playerbots/src/Ai/Class/Warrior/WarriorActions.cpp#L48-L73)) yields only to a **warrior** tank, so with no prot warrior in the roster the DPS warriors are the raid's Sunder providers. But Sunder has no trigger — it sits in `getDefaultActions()` (Fury 5.3, Arms 5.05) and only lands when nothing else wins the tick. Ramping to 5 stacks is therefore slow and at the mercy of whatever else is queued, and any new filler placed above the default band would starve it outright. |

### Outside the rotation

| # | Problem | Detail |
|---|---|---|
| G1 | **Weapon-speed preference never reaches the runtime gear path** | `ApplyPreferredSpecWeapons` ([StatsWeightCalculator.cpp:1068-1138](modules/mod-playerbots/src/Mgr/Item/StatsWeightCalculator.cpp#L1068-L1138)) returns `1.0f` unless `slot` is MAINHAND/OFFHAND/RANGED (`:1074-1077`). Only `PlayerbotFactory.cpp:2394,2429,2515` pass a slot. Every runtime caller — `ItemUsageValue.cpp:1020,1048,1050,1183,1264,1265` (loot + autogear upgrades), `EquipAction.cpp:164-304`, `BuyAction.cpp:81-132` — uses `CalculateItem(id, prop)`, slot defaulting to −1. The 3× spec-speed bonus therefore shapes initial factory gearing only; afterwards an Arms bot will swap its slow 2H axe for a fast 2H sword with marginally better stats, losing MS / Slam / Execute / Deep Wounds weapon scaling. Server config is not the issue — `AC_AI_PLAYERBOT_PREFERRED_SPEC_WEAPONS: "1"` is already set ([configurationOverrides/Playerbot.env:31](configurationOverrides/Playerbot.env#L31)). |
| G2 | **Arms PvE talent template spends 8 points in Protection** | `PremadeSpecLink.1.0.80 = 3022032023335100102012213231251-305-2033` (conf `:1686`) decodes to 55 Arms / 8 Fury / 8 Prot. The Prot points land in Improved Bloodrage / Improved Thunder Clap / Incite — no raid-DPS value. Those 8 belong in Fury (Armored to the Teeth, Commanding Presence). **Decode must be verified in-game before changing.** |
| G3 | **Melee hit cap ignores Precision** | `StatsWeightCalculator.h:21-26` hardcodes an 8 % melee hit cap for every melee class. Arms/Fury with Precision 3/3 cap at 5 %, so bots keep over-valuing hit and under-valuing crit/ArP. |
| G4 | **Glyphs unverified + PvP remap hazard** | Warrior glyph sets are raw item IDs in conf (`:1684,1688`); unfilled slots get a **random** eligible glyph ([PlayerbotFactory.cpp:4640-4681](modules/mod-playerbots/src/Bot/Factory/PlayerbotFactory.cpp#L4640-L4681)). The PvP remap at `:4404-4416` switches an Arms bot to the arms-**PvP** glyph set purely on the presence of Second Wind (29838). |

Explicitly **not** in scope:
- Sunder Armor stays in both DPS default lists (user decision).
  `CastSunderArmorAction::isUseful` already yields to a warrior tank and only refreshes below
  5 stacks or under 6 s ([WarriorActions.cpp:48-73](modules/mod-playerbots/src/Ai/Class/Warrior/WarriorActions.cpp#L48-L73)).
- Weapon stones. `InitConsumables` skipping the ladder above level 75 is intentional — leave as is.
- Flasks/elixirs and stat food. Handled server-side as world buffs, not by the bot factory.

---

## Plan

### Step 0 — findings document

Write `docs/general/warrior-arms-fury-dps-findings.md` containing the Findings section
above, with file:line references, so the analysis survives independently of this plan.
Matches the existing layout in [docs/general/](modules/mod-playerbots/docs/general/)
(`burst-cooldown-windows-plan.md`, `threat-redirect-trash-and-naxx-plan.md`).

### Step 1 — Fury rotation

[FuryWarriorStrategy.cpp](modules/mod-playerbots/src/Ai/Class/Warrior/Strategy/FuryWarriorStrategy.cpp)

- Add `target critical health` → `execute` at `ACTION_HIGH + 5` (25), matching Arms.
  Trigger already registered generically ([TriggerContext.h:47](modules/mod-playerbots/src/Ai/Base/TriggerContext.h#L47)).
- Move the Heroic Strike dump from `medium rage available` to `high rage available`
  (rage ≥ 60). **Leave its relevance at `ACTION_DEFAULT + 0.1`** — promoting it above the
  default band would push it past Sunder Armor (see Step 2a).
- Leave `getDefaultActions()` alone. Once Execute has its own trigger at 25, its 5.2 slot in
  the fallback list stops mattering, and Sunder Armor keeps its position.
- Bump `death wish` / `recklessness` to `ACTION_HIGH + 1` so they clear Cleave.

Target priority: bloodthirst > whirlwind > slam(Bloodsurge) > execute(<20%) >
death wish / recklessness > sunder armor(see 2a) > heroic strike(rage ≥ 60) > melee.

### Step 2a — make Sunder Armor deliberate (B5)

Applies to **both** DPS specs, and matters because this roster has no prot warrior.

Add a `SunderArmorStackTrigger` to [WarriorTriggers.h](modules/mod-playerbots/src/Ai/Class/Warrior/WarriorTriggers.h)
mirroring the conditions already in `CastSunderArmorAction::isUseful` — in a group, no warrior
tank present, and the debuff on the current target is below 5 stacks or under 6 s remaining.
The existing `SunderArmorDebuffTrigger` is **not** reusable: `DEBUFF_TRIGGER` fires only when
the aura is entirely absent, so it cannot drive the 1→5 stack ramp.

Register it under a new name in `WarriorAiObjectContext.cpp` (leave `"sunder armor"` pointing
at the existing trigger so `TankWarriorStrategy` is untouched) and push it from Arms and Fury at
`ACTION_HIGH - 1` (19) — below every rotational ability and both burst cooldowns, above all
filler. Because the trigger goes quiet at 5 stacks with >6 s left, the steady-state cost is one
refresh per ~24 s.

Net effect: Sunder reaches 5 stacks faster than today and is no longer hostage to fallback
ordering, while the single-target rotation is unaffected.

### Step 2 — Arms rotation

[ArmsWarriorStrategy.cpp](modules/mod-playerbots/src/Ai/Class/Warrior/Strategy/ArmsWarriorStrategy.cpp)
and [WarriorTriggers.h](modules/mod-playerbots/src/Ai/Class/Warrior/WarriorTriggers.h)

- Replace `MortalStrikeDebuffTrigger` in the Arms rotation with a cooldown-based trigger.
  Reuse the existing `CAN_CAST_TRIGGER` macro ([AiObject.h](modules/mod-playerbots/src/Bot/Engine/AiObject.h)) —
  same pattern as `OverpowerAvailableTrigger` — registered under a new name so the tank
  strategy's use of the debuff trigger is untouched. Relevance `ACTION_HIGH + 6` (26), above
  Execute.
- Drop `bladestorm` from `getDefaultActions()`; give it its own trigger below Execute /
  Overpower, or leave it to the AoE strategy only.
- Move Slam from `high rage available` to `medium rage available` and gate the action on
  Improved Slam being talented (`bot->HasSpell` on the Improved Slam ranks) — otherwise the
  1.5 s cast clips auto-attacks. Push it at `ACTION_HIGH - 2` (18), i.e. **below** the Sunder
  trigger from Step 2a, so the filler cannot starve the armor debuff.
- Delete the `intimidating shout` (A5) and `retaliation` (A7) trigger nodes from the Arms
  DPS strategy; move `enraged regeneration` from `medium health` to `critical health` (A6),
  matching Fury.
- Drop `rend on attacker` from the DPS rotation (A8), keeping plain `rend` on the current
  target.

Target priority: mortal strike > execute(<20% / Sudden Death) > overpower(TfB) > rend >
bladestorm > sunder armor(2a) > slam(filler) > heroic strike(high rage) > melee.

### Step 3 — off-GCD tick chaining (B1)

In [WarriorActions.cpp](modules/mod-playerbots/src/Ai/Class/Warrior/WarriorActions.cpp), give
Heroic Strike and Cleave an `Execute` override that calls `botAI->SetNextCheckDelay(0)` after a
successful cast so the same GCD window can still land a Bloodthirst / Mortal Strike.
`SetNextCheckDelay` is the established mechanism
([PlayerbotAIBase.cpp:32](modules/mod-playerbots/src/Bot/Engine/PlayerbotAIBase.cpp#L32));
raid actions already use it this way (e.g. [UldActions_Mimiron.cpp:64](modules/mod-playerbots/src/Ai/Raid/Uld/Action/UldActions_Mimiron.cpp#L64)).
Do the same for `bloodrage`, `death wish`, `recklessness` and `berserker rage`.

Keep the change warrior-local — do not touch `Engine::DoNextAction`.

### Step 4 — AoE strategy (B2)

[GenericWarriorStrategy.cpp:59-72](modules/mod-playerbots/src/Ai/Class/Warrior/Strategy/GenericWarriorStrategy.cpp#L59-L72)

- Remove `demoralizing shout without life time check`, `thunder clap` and `shockwave` from
  the `light aoe` node — they are prot abilities and either waste a GCD or can never cast for
  a DPS spec. Leave them for `TankWarriorStrategy`, which has its own nodes.
- Raise the trigger from `light aoe` (2) to `medium aoe` (3) so two-add boss fights keep the
  single-target rotation.
- Keep `sweeping strikes` and `bladestorm` (Arms) and `cleave` (Fury).

### Step 5 — pre-pull Battle Shout (B3)

Add a `battle shout` trigger node to
[GenericWarriorNonCombatStrategy::InitTriggers](modules/mod-playerbots/src/Ai/Class/Warrior/Strategy/GenericWarriorNonCombatStrategy.cpp#L33-L40).
Reuse the existing `BattleShoutTrigger` — it already suppresses itself when a stronger
Blessing of Might is up ([WarriorTriggers.cpp:101-166](modules/mod-playerbots/src/Ai/Class/Warrior/WarriorTriggers.cpp#L101-L166)).

### Step 6 — dead code (B4)

Delete `WarriorStanceRequirementActionNodeFactory` from
[GenericWarriorStrategy.h](modules/mod-playerbots/src/Ai/Class/Warrior/Strategy/GenericWarriorStrategy.h).
Confirm no references first.

### Step 7 — gear, talents, glyphs

- **G1 (weapon speed):** thread the equipment slot through the runtime scoring calls so
  `ApplyPreferredSpecWeapons` actually runs on loot/autogear/equip decisions, not just initial
  factory gearing. The callers already know which slot they are evaluating
  (`ItemUsageValue.cpp:1020,1048,1050,1183,1264,1265`, `EquipAction.cpp:164-304`,
  `BuyAction.cpp:81-132`) — they just drop it. No config change needed; the server already
  sets `PreferredSpecWeapons = 1`. Verify this does not destabilise non-warrior specs, since
  the function covers every class.
- **G3 (hit cap):** subtract talented hit (Precision for warriors) from the 8 % melee hit cap
  in `StatsWeightCalculator` before computing the overflow penalty.
- **G2 (talents) and G4 (glyphs):** verify the decoded Arms PvE talent link and the six
  warrior glyph item IDs against the DB **before** editing conf; then move the 8 Protection
  points into Fury and pin the glyph sets to the WotLK PvE choices (Arms: Mortal Strike,
  Rending, Execution; Fury: Whirlwind, Heroic Strike, Execution). Also guard the PvP glyph
  remap at `PlayerbotFactory.cpp:4404-4416` so a PvE build carrying Second Wind is not
  reclassified.

---

## Verification

1. **Build:** compile the module (the module can't be built headless in this environment — hand
   the branch off, or build locally, before claiming it works).
2. **Static read-through:** for each spec, list the final relevance table and confirm the
   intended priority order top-to-bottom with no ties between rotational abilities.
3. **In-game, `.playerbot` debug:** enable the `debug move` strategy plus playerbot action
   logging on one Arms and one Fury bot; pull a Naxx/Ulduar boss with a bot main tank and read
   the `A:<name> - OK` lines. Expect:
   - Fury: `bloodthirst` / `whirlwind` alternating, `slam` on every Bloodsurge proc,
     `execute` taking over below 20 %, `heroic strike` only when rage is high.
   - Arms: `mortal strike` roughly every cooldown (not every 10 s), `overpower` on every
     Taste for Blood, `execute` below 20 %, no `intimidating shout` / `retaliation`.
   - Neither: `demoralizing shout` during a 2-add boss phase.
   - **Sunder Armor:** with no prot warrior in the raid, confirm the boss reaches 5 stacks
     within the opening seconds and never drops below 5 for the rest of the fight. This is the
     main regression risk of Steps 1-2 and must be checked explicitly.
4. **DPS meter:** same boss, same gear level, before/after. Compare against a Retribution
   Paladin or Frost DK bot in the same raid as the control.
5. **Tank regression:** confirm a Protection warrior bot's rotation is unchanged — Steps 2, 4
   and 6 touch shared files.
