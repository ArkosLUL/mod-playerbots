# Revive Noth the Plaguebringer — gap analysis + rebuild

## Context

This started as a wipe-risk gap analysis of the current Noth the Plaguebringer bot strategy. The
finding that reframes the question: **there is no current Noth strategy.** Commit `ab1a203c4`
("Commented out Noth strategy") commented out 594 lines across 10 files without deleting anything.
Every Noth trigger, action, helper, multiplier and context entry is dead. The single live piece of
Noth-aware bot behaviour in the whole tree is the burst-window hold at
`src/Ai/Raid/Naxx/NaxxMultipliers.cpp:650`.

So the wipe risk splits in two, and this document covers both:

1. **What bots do on Noth today** — pure generic raid AI. Gaps G1–G6.
2. **What the disabled code would still get wrong if someone just uncommented it** — gaps G7–G13.
   Two of them are fatal in the same way Heigan's G1 was fatal: mechanics detected by watching for
   a cast the boss never observably performs.

Part 3 is the rebuild design. No code has been changed yet.

---

## Encounter facts (verified against `src/server/scripts/Northrend/Naxxramas/boss_noth.cpp`)

Phases alternate on fixed timers with **no HP gating**: ground 110 s, balcony 70 s, forever.

| Ground phase (110 s) | |
|---|---|
| t=10/40/70/100 s | Summon announce |
| t=14/44/74/104 s | Plagued Warriors spawn — `RAID_MODE(2, 3)` |
| t=15/40/65/90 s | Curse of the Plaguebringer (29213), `SPELLVALUE_MAX_TARGETS` = `RAID_MODE(3, 10)` |
| t=26/56/86 s | **25-man only**: `DoResetThreatList()` → Cripple (29212, non-triggered) → Blink (29208, triggered) |
| t=110 s | Teleport (29216) → balcony |

| Balcony phase (70 s) | |
|---|---|
| t=8/38/68 s | Add wave. Composition is keyed on `timesInBalcony`, which only increments on *exit*, so all 3 waves in one phase are identical |
| t=70 s | Teleport Back (29231), `timesInBalcony++`, back to ground |

| Balcony # | 10-man | 25-man |
|---|---|---|
| 1st | 2 Champions ×3 waves | 4 Champions ×3 |
| 2nd | 1 Champion + 1 Guardian ×3 | 2 + 2 ×3 |
| 3rd+ | 2 Guardians ×3 | 4 Guardians ×3 |

- Adds: Plagued Warrior 16984 (Cleave), Plagued Champion 16983 (Mortal Strike), Plagued Guardian
  16981 (AoE nuke). All SmartAI; `JustSummoned` calls `SetInCombatWithZone()` — every add aggroes
  the whole raid instantly. Spawns pick randomly from 5 fixed alcoves 25–50 yd out from the boss.
- Berserk 68378 fires once, on leaving the **third** balcony (`timesInBalcony == 3`) ≈ **540 s**.
- Hard leash: `IsInRoom()` evades past 80 yd from `(2684.8, -3502.5, 261.3)`. Instance boundary is
  `RectangleBoundary(2618.0, 2754.0, -3557.43, -3450.0)`.
- Balcony state is readable straight off the boss: `UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_DISABLE_MOVE`,
  rooted, `REACT_PASSIVE`.
- Curse of the Plaguebringer expiry triggers Unrelenting Plague (29214), raid-wide damage. Both are
  DBC-driven — no `SpellScript` is registered for any Noth spell. The ~10 s duration should be
  confirmed in-game before tuning any dispel deadline.

---

## Part 1 — gaps in what runs today

### G1 — The strategy does not exist (root cause)

All commented out: helper `NaxxBossHelper.h:1351-1445`, triggers `NaxxTriggers.h:386-404` +
`.cpp:521-534`, actions `NaxxActions.h:456-474` + the entire `Action/NaxxActions_Noth.cpp`
(328 lines, only the `#include`s are live), multiplier `NaxxMultipliers.h:172-180` +
`.cpp:406-439`, context entries `NaxxTriggerContext.h:70-71` and `NaxxActionContext.h:72-73`,
strategy nodes `NaxxStrategy.cpp:216-229` and `:246`.

Bots therefore run generic raid AI for the whole encounter. G2–G6 are what that generic AI does
wrong.

### G2 — Decurse throughput cannot keep up in 25-man (fatal)

The generic `"cure"` strategy *is* on by default for most roles (`AiFactory.cpp:307, 322, 340, 344,
346`), so decursing does happen. It is not fast enough:

- Priorities are **below** raid priority: mage `remove curse on party` 40.0f
  (`GenericMageStrategy.cpp:132`), druid 57.0f (`GenericDruidStrategy.cpp:92`); `ACTION_RAID` is
  60.0f (`Strategy.h:61`). Any raid positioning node outranks the dispel.
- `PartyMemberToDispel::Calculate` (`Value/PartyMemberToDispel.cpp:24-30`) returns **one** target
  per tick, ordered master → healers → tanks → others. One decurser clears roughly one target per
  GCD.
- 25-man puts the curse on **10 targets every 25 s** against a ~10 s window. A single mage or druid
  clears ~6 of 10 before Unrelenting Plague lands. Nothing splits the target list between multiple
  decursers, and paladins/priests cannot help at all (curse, not disease).

Also note `PlayerbotAI::HasAuraToDispel` skips auras with less than
`sPlayerbotAIConfig.dispelAuraDuration` remaining (`PlayerbotAI.cpp:4377-4379`, default 700 ms) —
small, but it is a hole at exactly the moment the curse matters.

### G3 — Blink threat reset is completely unhandled in 25-man (fatal)

`EVENT_BLINK` runs `DoResetThreatList()` on the entire list, then a hard-cast Cripple, then Blink.
Nothing in the module suppresses DPS or forces a re-taunt across that window. The bot with the
highest post-reset threat — a caster, or the healer that just topped the raid — takes the boss.

Partial mitigation exists by accident: `noth` is **not** in the `noRedirectBosses` blacklist
(`NaxxMultipliers.cpp:540-583`), so hunters and rogues do misdirect to the main tank. That is 3
shots' worth on a 30 s cadence, from at most a couple of bots.

### G4 — Main tank locks onto an add and never returns to the boss (fatal, most likely wipe today)

`AttackersValue::IsPossibleTarget` drops `UNIT_FLAG_NOT_SELECTABLE` units
(`Value/AttackersValue.cpp:156`), so Noth leaves `"attackers"` for the full 70 s balcony phase and
the main tank correctly retargets an add.

Coming back to ground is where it breaks. `FindTankTargetSmartStrategy::IsBetter`
(`Value/TankTargetValue.cpp:76-84`) short-circuits: an **explicit main tank in a group with more
than one tank sticks to `current target`** and rejects every alternative. `current target` is the
add it picked up on the balcony. With Warriors spawning every 30 s, the main tank can stay off the
boss for the rest of the fight. Noth then attacks whoever holds top threat.

The rest of `IsBetter` is fine for adds — `GetIntervalLevel` ranks units the tank has *no* aggro on
highest, which is correct pickup behaviour.

### G5 — No boss-tank / add-tank split, no cleave spread

Every add spawns with `SetInCombatWithZone()` from an alcove 25–50 yd away and runs at a random
raid member. There is no assignment separating "the tank on Noth" from "the tank collecting
Warriors", and no spacing rule against Plagued Warrior Cleave. Champions (Mortal Strike) and
Guardians (AoE nuke) land wherever they land — in practice, on healers.

### G6 — No add kill priority

`DpsTargetValue::Calculate` (`Value/DpsTargetValue.cpp:282-307`) picks by health / estimated group
DPS. Nothing prefers Guardians (the AoE nuke that actually kills the raid) over Warriors, and
nothing accounts for the 3rd+ balcony being up to 12 live Guardians in 25-man.

### Verified working — do not "fix" this

`NaxxBurstWindowMultiplier` (`NaxxMultipliers.cpp:648-653`) holds burst cooldowns while Noth is on
the balcony and spends them on the ground. It resolves the boss via `FindTargetValue`
(`Value/TargetValue.cpp:160-185`), which walks `GetThreatenedByMeList()` rather than `"attackers"`
— that list keeps Noth through the balcony phase and through `DoResetThreatList()`, so the check
holds in both phases. This is correct against the 540 s berserk and should survive the rebuild.

---

## Part 2 — gaps in the disabled code (blockers to a naive uncomment)

### G7 — It does not compile

`NaxxBossHelper.h:1409` calls `NaxxSpellIds::HasAnyAura(botAI, member, {...})` with three
arguments. The current signature takes two (`NaxxSpellIds.h:142`).

### G8 — The curse node points at an action that does not exist

`NaxxStrategy.cpp:227` — `NextAction("cure party member", ACTION_RAID + 2)`. Nothing registers that
name. `CurePartyMemberAction` is a base class (`GenericSpellActions.h:222`); its subclasses register
under spell names (`"remove curse"`, `"remove curse on party"`, `"cleanse spirit"`). The node
resolves to nothing and the curse handling is silently dead.

### G9 — The blink window is undetectable, so the blink branch never fires (fatal in 25-man)

`NothBossHelper::UpdateBossAI` stamps `_last_blink_ms` only when it catches the boss with
`UNIT_STATE_CASTING` and Blink in `CURRENT_GENERIC_SPELL`/`CURRENT_CHANNELED_SPELL`
(`NaxxBossHelper.h:1374-1394`). But the core casts Blink as **triggered and instant** —
`me->CastSpell(me, SPELL_BLINK, true)`. A triggered instant never sets `UNIT_STATE_CASTING` and is
gone before the next bot tick. `IsBlinkWindow()` effectively never returns true, so the entire
threat-reset protection in `NothGenericMultiplier` (`NaxxMultipliers.cpp:427-438`) is dead code.

Same failure shape as Heigan G1: the mechanic was wired to a cast the bots cannot observe.

The observable signal is the **Cripple** (29212) cast that immediately precedes it — non-triggered,
so it does occupy the cast slot — and/or the Cripple aura appearing on players. A 25-man-only timer
model anchored on ground-phase start (26 s, then every 30 s) is the robust backstop, same pattern
as `HeiganBossHelper`'s eruption clock.

### G10 — The curse branch zeroes 3 classes' entire output and boosts a dead action

`NothGenericMultiplier::GetValue` (`NaxxMultipliers.cpp:412-425`): while *any* curse is on *any*
group member, every druid, shaman and mage gets all actions zeroed except heals, with
`CurePartyMemberAction` boosted to 2.0f.

Combined with G8, the thing being boosted is a node that never resolves. In 25-man the curse is up
on someone roughly 10 s out of every 25 s, so this deletes ~40 % of three classes' uptime and
dispels nothing in exchange. Straight contribution to the 540 s berserk wipe.

### G11 — Position moves have no room clamp

`NothPositionAction` does raw `cos/sin` offset `MoveTo` in all four branches
(`NaxxActions_Noth.cpp:183-328`) — ranged kiting Champions out to 25 yd, assist tanks dragging adds
toward the nearest healer. Nothing constrains the destination to the encounter rectangle
(`2618..2754, -3557.43..-3450`) or the 80 yd evade leash. Bots can walk out of healer LOS or drag
the fight to the leash edge.

Kel'Thuzad already has the primitives to copy: `KelthuzadBossHelper::ClampToRoom`
(`NaxxBossHelper.h:283`) and `ComputeEscapeFromPoint` (`:452`).

### G12 — No melee handling on the balcony

Nothing repositions melee when the boss becomes untargetable and the only valid targets are adds
spawning 25–50 yd away at alcoves. Ranged get a kite branch; melee get nothing.

### G13 — Flat priorities

Both actions in the `"noth"` node share `ACTION_RAID + 1` (`NaxxStrategy.cpp:220-221`), so ordering
falls out of vector insertion order. Other bosses stagger deliberately (Kel'Thuzad +3/+2/+1). There
is also no emergency-priority action anywhere in Noth; compare Kel'Thuzad's shadow-fissure flee at
`ACTION_EMERGENCY + 6`.

---

## Part 3 — rebuild plan

Do **not** `git revert ab1a203c4`. Rewrite the helper and the multiplier; the target-selection
action is largely salvageable.

### 1. `NothBossHelper` — `NaxxBossHelper.h`, replacing the commented block at 1351-1445

Keep: `center`, the `UpdateBossAI` shape, `IsBalconyPhase()` (the unit-flag read is correct),
`GetAliveAssistTank()`.

Fix and add:
- `HasAnyAura(member, {...})` — 2-arg (G7).
- `GetBoss()` accessor, so `NaxxBurstWindowMultiplier::EvaluateWindow` can stop reading the raw
  flag and the two paths cannot drift. Every other Naxx helper exposes one.
- Replace `IsBlinkWindow()` with detection off **Cripple** (29212 cast or aura) plus a 25-man-only
  timer model anchored on ground-phase entry (26 s, then +30 s), following the `HeiganBossHelper`
  `PhaseState` pattern at `NaxxBossHelper.h:1265-1295` — instance-shared, re-anchored on phase
  edges, with a staleness guard. Gate on `bot->GetRaidDifficulty()`; in 10-man Blink and Cripple do
  not exist at all.
- `GetCursedMembers()` returning the list, not just a bool, so dispel duty can be split.
- A `ClampToRoom` equivalent bounded by the encounter rectangle, for every `MoveTo` destination.
- Balcony wave prediction from a `timesInBalcony` count tracked across phase edges, for add kill
  priority.

### 2. Curse handling — the G2 + G8 + G10 fix

- Register the trigger under a real action name. Follow `HeiganDispelDecrepitFeverAction`
  (`NaxxActions_Heigan.cpp:131-187`) — a Noth-specific dispel action with an explicit class switch
  (`remove curse` mage/druid, `cleanse spirit` shaman) beats trying to make `"cure party member"`
  resolve.
- Priority above `ACTION_RAID`. Heigan's dispel sits at `ACTION_RAID + 5`; Noth's should too.
- Split targets across decursers by index, the way `NaxxRedirectThreatAction::GetRedirecterIndex`
  (`NaxxActions_Shared.cpp:81-108`) splits redirect duty. Without this, N decursers all chase the
  same first target.
- Drop the blanket class suppression from the multiplier. Suppress *specifically* while that bot
  has a dispel to cast, not whenever anyone in the raid is cursed.

### 3. Tank behaviour — the G4 + G5 fix

- A `noth choose target` branch that pins the main tank back onto Noth the moment the boss clears
  `UNIT_FLAG_NOT_SELECTABLE`, overriding the `current target` stickiness in
  `TankTargetValue.cpp:76-84`.
- Explicit assist-tank ownership of Warriors during ground phase and of Champions/Guardians during
  balcony. The disabled `NothChooseTargetAction` (`NaxxActions_Noth.cpp:14-181`) already has this
  structure and is worth keeping largely as-is.
- Cleave spread for Warrior tanking — keep the 5 yd step-off branch, but route it through the room
  clamp (G11).

### 4. Blink response — 25-man only

On the Cripple/timer window: zero non-tank damage and debuff actions for the window and let the
main tank re-establish. Reuse the existing `NothGenericMultiplier` blink branch body
(`NaxxMultipliers.cpp:427-438`) — the logic is right, only the trigger was undetectable.

### 5. Add kill priority — the G6 fix

Guardians before Champions before Warriors, keyed on the balcony wave the helper is tracking.

### 6. Wiring — the 4 sites

`NaxxTriggerContext.h:70-71`, `NaxxActionContext.h:72-73`, `NaxxStrategy.cpp` `InitTriggers`
(staggered priorities, G13) and `InitMultipliers` (`:246`).

### Difficulty coverage

Both modes. The real split: Blink/Cripple are 25-man only; curse targets 3 vs 10; adds
`RAID_MODE(2,3)` / `RAID_MODE(2,4)`. Gate on `bot->GetRaidDifficulty()` the way
`RazuviousTankTrigger` does (`NaxxTriggers.cpp:143-161`).

---

## Verification

The module cannot be compiled headless in this environment, so verification is static review plus
an in-game pass.

**Static**
- Confirm the 4 wiring sites resolve: every `NextAction` name in the Noth nodes has a matching
  `creators[...]` entry, and every trigger name too. This is exactly what G8 was.
- Confirm no remaining 3-arg `NaxxSpellIds::HasAnyAura` calls.
- Confirm every `MoveTo`/`MoveInside` destination in `NaxxActions_Noth.cpp` passes through the room
  clamp.

**In-game, 10-man**
1. Pull, watch through two full ground↔balcony cycles. Main tank must be back on Noth within a few
   seconds of each ground return (G4).
2. Confirm the Cripple-window suppression never engages — Blink does not exist in 10-man.
3. Curse: with one mage or druid, all 3 targets clear before expiry.
4. Adds picked up by the assist tank, not left free on healers.

**In-game, 25-man**
5. Blink at ground-phase t≈26/56/86 s: DPS stops for the window, main tank re-establishes, no bot
   dies to a free boss.
6. Curse on 10 targets: measure how many clear before Unrelenting Plague. That number tells you
   whether the index-split in step 2 is sufficient or whether decurse needs its own emergency
   priority.
7. 3rd balcony (12 Guardians): confirm kill priority and that no bot leaves the encounter rectangle.
8. Kill before the 540 s berserk, with burst cooldowns spent on the ground and held on the balcony.
