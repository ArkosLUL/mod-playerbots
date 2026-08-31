# Freya — Detonating Lasher rework (replacing the corral)

## Context

Four Freya hard-mode pulls were run on 2026-08-31 (`env/dist/logs/botobs/603_1_elder-stonebark_*`).
All four wiped. The two most recent are the subject here:

| trace | length | deaths | raid dps | Attuned stacks removed |
|---|---|---|---|---|
| `…_1788200810` | 222 s | 28 | 32.8 k | 59 / 150 |
| `…_1788201407` | 132 s | 26 | 25.5 k | 12 / 150 |

(the two before them: 50 s / 29 deaths / 0 stacks, and 97 s / 26 deaths / 20 stacks)

The Detonating Lasher corral shipped in `20bc3b43e` (2026-08-25) is **net-negative** and is the
proximate cause of most deaths. This plan removes it and replaces it with the strategy real raids
use. Intended outcome: a lasher wave costs the raid a burst of healing instead of a third of its
roster, and add throughput roughly doubles.

## What the traces prove

### 1. Detonate is not what kills anyone

Damage taken in `…_1788201407` (132 s):

| source | share |
|---|---|
| Detonating Lasher **melee** | 34.4 % |
| Detonating Lasher **Flame Lash** | 12.9 % |
| Detonating Lasher **Detonate** | 10.4 % |
| Freya Ground Tremor | 11.8 % |
| Freya melee | 11.7 % |
| Ancient Conservator Nature's Fury | 6.5 % |

Lashers are **57.7 %** of all raid damage, and melee + Flame Lash outweigh Detonate **4.6 : 1**.
In `…_1788200810` Detonate was 2.3 %. The corral spends the raid's positioning budget on the
smallest slice.

Detonate is also **melee-only in practice**: of 282 471 Detonate damage, 250 458 (89 %) landed on
the eight melee bots and **zero** on ranged. Ranged were never at risk from the mechanic the corral
exists to dodge.

### 2. The corral is the leading cause of death

Deaths whose `lastmove.by` was a corral action:

- `…_1788200810`: **13 of 28** (46 %) — 12 `freya drag lasher to corral`, 1 `freya trap lasher corral`
- `…_1788201407`: **10 of 26** (38 %) — 9 drag, 1 trap

**All four healers died mid-drag in both pulls** (`Elemena`, `Prayer`, `Tree`, `Holylight`, every
one `[STILL WALKING]` toward the corral). `FreyaDragLasherToCorralTrigger` admits healers by design
(`IsRanged(bot) || IsHeal(bot)`).

Add `freya avoid detonating lasher` — the 5500-HP flee — and **17 of 26** deaths in the last pull
happened while the bot was running from a lasher.

The trap hunter is a designated victim: `Trueshot` died at its post in both pulls, once with four
lashers inside 4 yd.

### 3. Dragging cannot work, for a physical reason

`speed_run` 1.14286 → **8.0 yd/s** against a player's 7.0. The lasher never falls behind. A dragger
therefore walks the full 35 yd taking uninterrupted melee, out of healer range, dealing nothing.
Target share during the wave confirms it: the draggers held **no target** 45–74 % of the time
(`Trueshot` 74 %, `Power` 56 %, `Smartface` 48 %, `Agony` 45 %).

### 4. The corral's own payoff never materialised

`freya frost nova lashers` and `freya lasher pack step out` **do not appear once** in either trace.
The pack requires 6 lashers inside 10 yd of a bot; draggers died or were pulled off before it ever
formed. The nova and the step-out have never executed in production.

The corral point is also not stable between pulls — `(2406.44, -9.55)` in one, `(2390.3, -20.16)` in
the next — because `GetHomePosition()` tracks where Freya was left, and she drifts ~13 yd while tanked.

### 5. The raid was never stacked

Largest group inside one 10 yd circle was ≤ 8 bots for half the pull and ≤ 5 for a quarter of it.
No AoE opportunity was ever created.

### 6. Wrong spell id in our code and docs

Detonate in 25-man is **62937** (base 6824). `UldBossHelper.h:861` and `docs/raids/ulduar.md:811`
name only **62598**, the 10-man version (base 4162). Both carry `EffectRadiusIndex 18` = **15 yd**,
so the radius constant is right and only the id is wrong.

### 7. Ground Tremor is a raid-wide interrupt, and it lands

`62859` (25-man; `62437` is the 10-man twin, same split as Detonate) is not just damage:
`Effect_1 = 2` school damage 7599 physical, **`Effect_2 = 68` SPELL_EFFECT_INTERRUPT_CAST** with
`EffectMechanic_2 = 26`, both at `EffectRadiusIndex 28` = **50000 yd** — raid-wide, nothing to dodge.
`DurationIndex 1` = a **10 s school lockout** via `ProhibitSpellSchool` in `Spell::EffectInterruptCast`.

Measured over the 7 volleys of `…_1788200810` (~30 s apart; the script repeats it `25s..35s`, so it is
**not** a fixed timer):

- 37 cast-time casts were in flight when a volley landed
- **31** had `PreventionType == SPELL_PREVENTION_TYPE_SILENCE`, so they were cut and school-locked
- only **9 of 31** landed a same-school cast again inside the next 10 s — **71 % stayed locked out**

It hits the people who can least afford it. `Elemena` (resto shaman) was mid-Chain Heal at four of the
seven volleys; Nature-locked for 10 s is a resto shaman with nothing. `Tree` (Healing Touch) and
`Holylight` (Holy Light) the same. Hunters are exempt — Steady Shot is `PreventionType = 2` (PACIFY),
which never reaches the lockout branch.

**It is fully telegraphed.** The trace carries seven `cast` records, caster Freya, spell 62859,
`ct = 2000` ms, each landing ~2.1 s later. No timer prediction is needed and none would work.

Hard mode only: `EVENT_FREYA_GROUND_TREMOR` is scheduled solely when Elder Stonebark is alive.

## Encounter facts worth having (verified this session)

- `JustEngagedWith` sets **Attuned to Nature (62519) to 150 stacks**; it is +8 % healing received per
  stack, so Freya is untouchable until adds strip it. Both pulls left her at **100.0 %**.
- Stack value per add: Detonating Lasher **2**, Storm Lasher / Snaplasher / Water Spirit **10**,
  Ancient Conservator **25**.
- HP per stack — Detonating Lashers are the **worst value in the fight**:
  Water Spirit 66.9 k · Storm Lasher 100.4 k · Conservator 113.8 k · Snaplasher 125.5 k ·
  **Detonating Lasher 150.6 k** (301 212 HP for 2 stacks; 3.01 M for a full wave of 10).
- Waves: `EVENT_FREYA_ADDS_SPAM` at 10 s then **every 60 s, six waves**. Clearing a wave
  (`_aliveAddsCount == 0`) pulls the next in after **5 s**. After the sixth wave the aura is stripped
  regardless, so the fight is a **survival** check, not purely a DPS race.
- Hard mode is on (`AC_AI_PLAYERBOT_ULDUAR_FREYA_HARD_MODE=1`). The three Elders are
  `NOT_SELECTABLE` + `REACT_PASSIVE` + banished, channelling into Freya — **they cannot be attacked**,
  so their 100 % HP is correct. They add Ground Tremor (35 s), Iron Roots (20 s) and Unstable Sun
  Beam (60 s). Ground Tremor has radius index 28 = **50000 yd, raid-wide and unavoidable**.

## What real raids do

Guides agree, and it is the opposite of the corral:

- Detonating Lashers **cannot be taunted or tanked** and fixate on random players. Nobody kites them.
- **Stack the raid in one spot** so the lashers gather themselves, and **AoE them down together**.
- **Do not let them all die at once.** Burn the pack to low health, then stop AoE, **slow and root
  them**, move the raid away, and **single-target them down one at a time** so the 15 yd detonations
  stagger and healers can top up between each.

Sources: [Warcraft Tavern — Freya Ulduar 25](https://www.warcrafttavern.com/wotlk/guides/freya-strategy-guide-ulduar-25/),
[Icy Veins — Freya encounter guide](https://www.icy-veins.com/wotlk-classic/freya-encounter-guide-strategy-abilities-loot),
[Warcraft Wiki — Freya (tactics)](https://warcraft.wiki.gg/wiki/Freya_(tactics))

The existing `GetFreyaRangedLasherFocus` (lowest-HP-first) is already the *finisher* half of that.
What is missing is the *gather and AoE* half — and the corral actively prevents it.

## Settled decisions (do not relitigate)

- **Doctrine: stack, AoE, then step out.** Ranged and healers hold one camp so the lashers gather
  themselves; the raid AoEs the pack down; at 20 % AoE stops, the mage roots it and the hunter snares
  the lane, ranged and healers move past 16 yd, and the existing lowest-HP focus picks them off one at
  a time so the detonations stagger.
- **The 5500-HP flee is repurposed, not kept.** `FreyaAvoidDetonatingLasherAction` and
  `FreyaLasherPackStepOutAction` are already byte-for-byte the same behaviour (both sweep every lasher
  in `ULDUAR_FREYA_HAZARD_SEARCH_RADIUS` and call `FindNearestPositionClearOfHazards` at 16 yd), so the
  avoid node is deleted and the pack step-out is the single survivor.
- **Scope: the lasher wave, plus the Ground Tremor pre-cast gate**, and a separate investigation into
  what is left of raid throughput. Wave overlap and `ResolveFreyaDpsTarget`'s Conservator-over-lasher
  priority are explicitly out.
- **The Ground Tremor gate copies Ignis, it does not invent anything.**
  `IgnisFlameJetsHoldCastMultiplier` (`UldMultipliers.cpp:550-590`) is already this exact gate — watch
  the boss's cast, cache the remaining window per millisecond, allow any candidate whose `CalcCastTime`
  still lands first — paired with `IgnisFlameJetsHoldCastAction` for a cast already in flight. Freya's
  version is the same shape with different ids.
- **No new world coordinate.** The camp anchors on a live bot, so the "never invent a raid coordinate"
  rule has nothing to probe — that is the point of choosing an anchor bot over a fixed point.

## Implementation

Save this document to `modules/mod-playerbots/docs/plans/freya-lasher-stack/freya-lasher-stack.PLAN.md`
first, then work from there.

### 1. `src/Ai/Raid/Uld/Util/UldBossHelper.{h,cpp}`

Delete `ULDUAR_FREYA_LASHER_CORRAL_DISTANCE`, `_CORRAL_ARRIVE`, `_CORRAL_COMMIT`,
`ULDUAR_FREYA_LASHER_TRAP_OFFSET`, `ULDUAR_FREYA_DETONATE_FLEE_HEALTH`, and the helpers
`GetFreyaLasherCorral`, `GetFreyaLasherTrapPost`, `GetFreyaLasherChasing`.

Keep `ULDUAR_FREYA_DETONATE_RADIUS` (15), `ULDUAR_FREYA_LASHER_PACK_CLEAR` (16),
`ULDUAR_FREYA_FROST_NOVA_RADIUS` (10), `IsFreyaLasherTrapHunter`, `CountFreyaLashersNear`.

New constants:

```cpp
constexpr float ULDUAR_FREYA_LASHER_PACK_RADIUS = 8.0f;   // AoeTrigger's own radius
constexpr float ULDUAR_FREYA_LASHER_FINISH_PCT = 20.0f;
constexpr float ULDUAR_FREYA_RANGED_CAMP_TOLERANCE = 10.0f;
constexpr float ULDUAR_FREYA_HEALER_CAMP_TOLERANCE = 15.0f;
```

`ULDUAR_FREYA_LASHER_PACK_MIN_COUNT` drops **6 → 3**, matching `MediumAoeTrigger`: three lashers
inside 8 yd is both what makes class AoE fire and roughly a squishy's health in chained Detonate.

New helpers:

- `Player* GetFreyaRangedCampAnchor(PlayerbotAI* botAI)` — lowest-GUID living ranged-DPS bot in the
  group on this map, the same tie-break `IsFreyaLasherTrapHunter` uses. Everyone gathers on it, so the
  camp is always on the mesh, always in range, and needs no coordinate.
- `Unit* GetFreyaLasherPackFocus(FreyaWaveState const& state)` — the lasher with the most living
  lashers within `ULDUAR_FREYA_LASHER_PACK_RADIUS`, lowest GUID breaking ties. The AoE-phase focus:
  aiming here is what satisfies `AoeTrigger`, which counts attackers within 8 yd of the **current
  target**, not of the bot.
- `bool IsFreyaLasherPackFinishing(FreyaWaveState const& state, Position const& centre)` — at least
  `PACK_MIN_COUNT` lashers within `PACK_RADIUS` of `centre`, every one at or below `FINISH_PCT`.

### 2. `src/Ai/Raid/Uld/Trigger/UldTriggers_Freya.{h,cpp}`

Delete `FreyaDragLasherToCorralTrigger` and `FreyaAvoidDetonatingLasherTrigger`.

New `FreyaRangedCampTrigger`: ranged DPS or healer, not a tank, not the anchor itself, lashers alive,
and the bot is farther than its role's tolerance from the anchor.

Retarget the three survivors from "6 lashers near me" to "a finishing pack near me":

| trigger | new condition |
|---|---|
| `FreyaLasherPackStepOutTrigger` | ranged or healer, `IsFreyaLasherPackFinishing` within `PACK_CLEAR` |
| `FreyaFrostNovaLashersTrigger` | mage, can cast, pack finishing within `FROST_NOVA_RADIUS` |
| `FreyaTrapLashersTrigger` (was `…CorralTrigger`) | trap hunter, can cast, pack finishing within `PACK_CLEAR` |

### 3. `src/Ai/Raid/Uld/Action/UldActions_Freya.{h,cpp}`

Delete `FreyaDragLasherToCorralAction`, `FreyaAvoidDetonatingLasherAction`, and
`FreyaTankAddsAction::HoldLasherCorral` (the tank goes back to its normal add ladder).

New `FreyaRangedCampAction`, modelled on `HodirRaidPositionAction`
(`src/Ai/Raid/Uld/Action/UldActions_Hodir.cpp`): no arrival latch, `MoveTo(...)` at
`MovementPriority::MOVEMENT_COMBAT` so a dodge can still outrank it — the trigger standing down inside
the tolerance is what stops the churn.

`FreyaTrapLasherCorralAction` → `FreyaTrapLashersAction`: drop the walk to the post entirely, keep
only `botAI->CastSpell("frost trap", bot)`. Because the trap node sits *below* the step-out, the hunter
has already moved when it fires, which lands the patch on the lane between the pack and the raid —
where the old design wanted it, without a post to walk to.

`FreyaLasherPackStepOutAction` and `FreyaFrostNovaLashersAction` are unchanged.

In `FreyaSetDpsPriorityAction::ResolveFreyaDpsTarget`, the ranged lasher pick becomes two-phase:
`GetFreyaLasherPackFocus` while the pack is above `FINISH_PCT`, then the existing
`GetFreyaRangedLasherFocus` (lowest HP) to finish. The `reach` guard around it stays as is — ranged
still never walk to a focus they cannot already hit.

### 4. `src/Ai/Raid/Uld/UldMultipliers.{h,cpp}`

New `FreyaLasherFinishAoeMultiplier`: `0.0f` for `action->getThreatType() == Action::ActionThreatType::Aoe`
(exempting `CastHealingSpellAction`) while a pack near the bot is finishing. This is the guide's "stop
AoE at low health" — without it the raid blows the whole pack up at once. Pattern to copy:
`src/Ai/Dungeon/GD/GDMultipliers.cpp:30`.

### 5. Ground Tremor pre-cast gate

Freya telegraphs Ground Tremor with a 2000 ms cast, so a bot has ~2 s to decide. Two halves, both
mirroring the Ignis Flame Jets trio.

Spell ids into the enum beside `SPELL_ATTUNED_TO_NATURE` (`UldBossHelper.h:118`) — **both**, as with
Detonate, because 62437 is 10-man and 62859 is 25-man:

```cpp
SPELL_FREYA_GROUND_TREMOR_10 = 62437,
SPELL_FREYA_GROUND_TREMOR_25 = 62859,
```

**Helper** (`UldBossHelper.{h,cpp}`), mirroring `IsIgnisFlameJetsCasting`:

```cpp
// Freya's only telegraph worth reacting to: a 2 s cast that interrupts the whole raid and locks the
// school for 10 s. Matches both difficulty ids - 62437 is the 10-man twin.
bool IsFreyaGroundTremorCasting(Unit* boss);
```

**Multiplier** `FreyaGroundTremorCastGateMultiplier`, copied from `IgnisFlameJetsHoldCastMultiplier`:
cache the remaining window per millisecond, resolve the candidate through
`AI_VALUE2(uint32, "spell id", spellAction->getSpell())` and `SpellInfo::CalcCastTime()`, return `1.0f`
for anything instant or that still lands first, `0.0f` otherwise. Exempt the hold action by name, as
Ignis does.

Unlike Ignis, **heals are gated too, and that is the point**: the volleys caught three healers
mid-cast repeatedly, and holding a Chain Heal for under 2 s beats losing Nature for 10. Bots whose
long cast is blocked fall through to the instants already in their rotation.

**Action + trigger** `freya ground tremor hold cast`, copied from `IgnisFlameJetsHoldCastAction`:
stop a cast already in flight that Ground Tremor would eat. Worth more here than at Ignis — Ignis only
reclaims the mana, whereas `Spell::EffectInterruptCast` applies `ProhibitSpellSchool` **only if it
finds a cast to cut**, so stopping first dodges the 10 s lockout outright. Node at
`ACTION_EMERGENCY + 2`, matching the Ignis node in `UldStrategy.cpp:130-132`.

### 6. Wiring

`UldStrategy.cpp` — delete the `freya drag lasher to corral` and `freya avoid detonating lasher` nodes,
rename the trap node, add the camp. Order is load-bearing:

| node | priority | why there |
|---|---|---|
| `freya frost nova lashers` | `ACTION_RAID + 4` | root the pile before anyone leaves it |
| `freya lasher pack step out` | `ACTION_RAID + 3` | then leave |
| `freya trap lashers` | `ACTION_RAID + 2` | below the step-out, so the patch lands on the exit lane |
| `freya ranged camp` | `ACTION_RAID` | last: gather only when nothing urgent is asking |

Register the camp, the renamed trap and the Ground Tremor hold in **both** the `creators[...]` map and
the static factory of `UldTriggerContext.h` / `UldActionContext.h`, and push the new multiplier beside
`FreyaTrioSyncMultiplier` at `UldStrategy.cpp:874-875`. No CMake change — every file already exists.

### 7. Docs

Run `/compact-docs-writer` before touching either doc (they are reachable from the module `CLAUDE.md`).

- `docs/raids/ulduar.md` Freya lasher block: replace the corral description with this doctrine, and
  correct the facts the traces disproved — Detonate is **62937** in 25-man (6824 base) and 62598 in
  10-man, 15 yd either way; it lands on melee and essentially never on ranged; melee and Flame Lash are
  4.6× Detonate; a lasher at 8.0 yd/s cannot be walked anywhere by a 7.0 yd/s bot. Add the wave
  economics (150 stacks, 2 per lasher, 150.6 k HP per stack — the worst rate in the fight) and the
  cadence (10 s then every 60 s, six waves, next wave in 5 s if one is cleared early).
- `UldBossHelper.h:861`: name both spell ids.
  Also document Ground Tremor properly: raid-wide interrupt plus 10 s school lockout on a 2 s
  telegraph, `25s..35s` repeat, hard mode only, and that hunters are exempt via `PreventionType`.
- `docs/engine/raid-mechanics-lessons.md`: three general lessons — read `speed_run` before designing
  any relocation, because an add faster than a player can be neither kited nor ferried; confirm which
  spell id the *difficulty in play* actually uses before building a rule on its numbers; and read a
  boss spell's full effect list, since a "damage" spell carrying `Effect 68` silences the raid and the
  damage number says nothing about that.
- Delete `docs/plans/freya-lasher-corral/` — that plan describes work being removed.

### 8. Separate: raid throughput investigation

25 bots produced 25–33 k. Investigation first, findings doc, then ask before any code, using the
traces already on disk:

- Whether `PlayerbotAI::CanCastSpell` respects an active school lockout. If it does not, a locked-out
  bot keeps re-offering a spell it cannot cast instead of falling through to instants, and the gate
  above only covers the casts it prevents rather than the 10 s that follow one it missed. Confirm
  rather than assume.
- Time-on-target from `snap.u[7]` once the corral is gone — how much of the residual "no target" is
  structural rather than the drag.
- A non-add baseline from `603_1_kologarn_*` or `603_1_xt-002-deconstructor_*` for DPS per head, to
  separate "Freya is hostile to bots" from "bots are slow everywhere".
- The `act` stream for rotation actions returning FAILED / IMPOSSIBLE.
- Gear and spec of the 25.

Findings go to `docs/engine/` (raid-agnostic), not `docs/raids/ulduar.md`.

## Verification

The module cannot be compiled headless here, so the build is a hand-off.

1. Static: `grep -rn "corral\|Corral" src/` returns nothing; each new name appears in exactly three
   places (context map, factory, class); no reference survives to `ULDUAR_FREYA_DETONATE_FLEE_HEALTH`.
2. Build the worldserver with the module.
3. Lasher wave, 25 HM. Ranged and healers form one loose ball instead of scattering; nobody walks a
   lasher anywhere; melee stay on the boss.
4. Confirm three or more lashers cluster in the ball and that class AoE actually fires (`act` stream
   shows Blizzard / Consecration / Death and Decay, not just single-target).
5. At 20 % confirm AoE stops, the mage novas, the hunter's trap lands between pack and raid, and ranged
   plus healers move past 16 yd while melee keep swinging.
6. Confirm lashers then die one at a time rather than together.
7. Ground Tremor: watch a volley land and confirm bots stop or withhold long casts through the 2 s
   telegraph, and that healers are casting again immediately after instead of sitting out 10 s.
8. RaidObs on the new trace: `--clump` shows a larger sustained group than the ≤ 8 measured here;
   deaths whose `lastmove.by` is a lasher node should be near zero (was 13/28 and 10/26); total lasher
   damage share should fall well below 57 %; stacks-per-minute should beat the 5.5/min of the best of
   these four pulls. Re-run the interrupt count — of the casts in flight at a volley, far fewer than
   31 should be interruptible, and the same-school-within-10 s recovery should rise well above 9/31.
9. Repeat in 10-man and with `AiPlayerbot.UlduarFreyaHardMode = 0` (which removes Ground Tremor
   entirely, so the gate must simply never fire there).
