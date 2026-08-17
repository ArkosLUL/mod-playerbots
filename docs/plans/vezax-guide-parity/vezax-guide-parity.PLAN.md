# General Vezax — guide parity rework

## Why

Vezax is the thinnest non-trivial Ulduar encounter: 6 triggers / 6 actions, ~210 lines, no
multiplier, no helper layer, no positioning. It predates the current conventions and was never
revisited while Hodir, Mimiron, Freya and Kologarn were reworked.

Audited against the Warcraft Tavern Ulduar-25 guide, the encounter script
`src/server/scripts/Northrend/Ulduar/Ulduar/boss_general_vezax.cpp`, the DBC reference in
`modules/mod-spell-tweaks/data/dbc-reference/`, and `docs/raids/README.md` + `docs/engine/*.md`:
**three of the six existing nodes do the wrong thing, two of them actively harmful**, and the
fight's defining mechanic — mana economy under Aura of Despair — exists only as a cheat.

Outcome: Vezax fights as the guide describes, on both raid sizes, with a hard mode that does not
break itself, and no dependency on `BotCheats = raid`.

## Verified encounter facts

Script for flow and timers, DBC CSVs for spell values, `acore_world` for creature templates.
**These override the public guides where they disagree, and they do disagree.**

| Spell | Id | Effect |
|---|---|---|
| Aura of Despair | 62692 → 64848 | No mana regen, −20% melee attack speed. On aggro. |
| Shadow Crash (cast) | 62660 | Every 10 s from 13 s. Random player **>3 yd combat reach**, falling back to *any* target — it can land on the tank. |
| Shadow Crash (impact) | 62659 | 11310 damage + knockback, **radius 10 yd**. Instant on missile landing — unreactable. |
| Shadow Crash (field) | 63277 → 65269 | **8 yd, 20 s.** +100% magic damage done, +75% shadow damage done, +100% cast speed, −70% mana cost, **−75% healing done**. `SPELL_EFFECT_PERSISTENT_AREA_AURA` → exists as a `DynamicObject`. |
| Searing Flames | 62661 | On the tank, **radius 100 yd** = whole raid. 13875–16125 fire + −75% armour 10 s. Every **8 s (25m) / 15 s (10m)**. **2000 ms cast, `PreventionType = 1` (silence)** — genuinely interruptible. Not cast while Saronite Barrier is up. |
| Surge of Darkness | 62662 | 63 s from pull, repeats 63 s. +100% physical damage, −55% move speed, 10 s. Delays the Searing Flames event group 10 s. |
| Mark of the Faceless | 63276 → 63278 | 20 s from pull, repeats 40 s, lasts 10 s. Drains 5000 hp/s from allies **within 15 yd** and heals Vezax. Prefers a player **>15 yd from the boss** when ≥9 (25m) / ≥4 (10m) are out there, else someone inside 15 yd. |
| Saronite Vapors (NPC 33488) | summon 63081 | Every 30 s. `NullCreatureAI`, `creature_template_addon.auras = NULL`, `MoveRandom(4.0f)` — **the living cloud is harmless**. |
| Saronite Vapors (puddle) | 63323 (30 s) → 63322 | Dropped on the **corpse**. 8 yd, reapplied every 4 s. Deals `100 * 2^stacks`, returns **half as mana**. Stack 5 = 3200, stack 8 = 25600. |
| Saronite Animus | NPC 33524 | Hard mode. At vapor #6 with none killed, all vapors charge to `(1852.78, 81.38, 342.461)` and merge. |
| Saronite Barrier | 63364 | −99% damage taken on Vezax until the Animus dies. |
| Profound Darkness | 63420 | Animus self-cast every 2 s. 749 damage + **+10% shadow damage taken per stack, 180 s**. **Radius index 28 = 50000 yd — room-wide, unavoidable.** |
| Berserk | 26662 | 10 min. Also instant if the boss leaves `x ∈ [1720,1940]`, `y ∈ [20,210]`. |

- **Killing any single vapor calls `DoAction(1)` and disables hard mode permanently** for that pull.
- Guides say to keep interrupting Searing Flames during the Animus. **On this core you do not** —
  `boss_general_vezax.cpp:228` skips the cast while the Barrier is up.
- Guide positioning: tank holds the boss with melee behind it, ranged in a wide loose arc, healers
  separated. Caster DPS stack **into** the purple Shadow Crash field; healers and mana-starved
  casters dip into the **green** vapor puddle; a marked player steps away alone.

## Engine facts this design depends on

- **Class interrupts sit at `ACTION_INTERRUPT` (40), below `ACTION_RAID` (60).** Every class has an
  `InterruptSpellTrigger` wired per-spec (rogue kick `+2`, pummel, wind shear, spell lock, silencing
  shot, shield bash), all firing only when `"current target"` is the caster. A bot mid-move returns
  `true` from its positioning action and starves every interrupt that tick — **the "return `false`
  once parked" discipline is what keeps Searing Flames interruptible at all.**
- `GetDynamicObjectPositions(bot, radius, spellId)` (`src/Ai/Raid/RaidBossHelpers.cpp:283`) finds
  persistent area auras; Karazhan uses it for Charred Earth.
- `MoveAwayFromCreatureAction::Execute` has a latent bug at
  `src/Ai/Base/Actions/MovementActions.cpp:2846`: the filter reads `(alive && unit->IsAlive())`, so
  passing `alive = false` matches nothing and the action silently never fires. **Deliberately left
  alone** — no caller passes `false` today, so the parameter is inert rather than harmful, and this
  encounter has no reason to touch a base action. Vezax scans the grid for vapor corpses itself.
  Worth a separate change if anything ever needs corpse matching.
- **Ulduar has no `AddSC_UlduarBotScripts()`.** Six raids have one, registered in
  `src/Script/Playerbots.cpp:513-539`.

## Findings

### Sev 1 — nodes that are wrong

**F1. The Shadow Crash field is treated as a hazard; it is the fight's main resource.**
`VezaxShadowCrashTrigger` fires on `HasAura(63277)` and strafes the bot out. 63277 is the *lingering
field* — 62659 already landed and cannot be dodged. The bot walks out of +100% magic damage, +100%
cast speed, −70% mana cost, which is the answer to Aura of Despair. Same failure class as the
Mimiron Laser Barrage lesson: dodging the wrong spell id.
`UldTriggers_Vezax.cpp:37-46`, `UldActions_Vezax.cpp:41-82`.

**F2. The Profound Darkness dodge is a no-op.** 63420 is room-wide (50000 yd).
`ULDUAR_VEZAX_PROFOUND_DARKNESS_RADIUS = 15.0f` is a guess (`ulduar.md:63` says so) and is wrong.
The node costs every ranged bot and healer uptime during the hard-mode burn.

**F3. Vapor avoidance targets the harmless object and misses the real hazard.** NPC 33488 alive has
no auras and no AI. The hazard is the corpse puddle. Both `TooCloseToCreature` and
`MoveAwayFromCreatureAction` default `alive = true`, so the corpse is never seen. Recorded Sev-1 gap,
`ulduar.md:1150`.

**F4. Hard mode breaks itself.** Nothing stops a bot killing a vapor, and one kill ends hard mode.
Vapors spawn on the boss every 30 s and wander into the melee stack where every AoE and DoT lands.
(Opt-in, default off — hence stage 3.)

**F5. Mana economy is cheat-only.** `VezaxCheatAction` sets `POWER_MANA` to max. CHEAT-ONLY at
`ulduar.md:1166`; without `raid` in `BotCheats` the fight is unwinnable for healers.

### Sev 2 — missing mechanics

**F6. No positioning.** Shadow Crash's 10 yd impact hits a clump; Mark of the Faceless drains
5000/s per ally within 15 yd *and heals the boss*; and since the core prefers marking someone >15 yd
out only when ≥9 (25m) / ≥4 (10m) are there, a clumped raid guarantees the mark lands in melee.
**F7. Surge of Darkness unhandled** — the only real tank-death window.
**F8. Searing Flames interrupt is incidental** — no coordination, and a moving bot suppresses its
own kick.
**F9. No hard-mode burst window.** **F10. No berserk or arena-bound awareness.**

### Sev 3 — convention gaps

**F11.** Mark spot is a **73 yd run** to a hardcoded corner `(1913.65, 122.94, 342.38)` for a 15 yd
mechanic, no navprobe note.
**F12.** Flat ladder, no rationale — the mana cheat and the Mark step-out are both `ACTION_RAID`.
**F13.** Boss resolved by name string in all six triggers; a bot that switches to the Animus or a
vapor leaves the threat list and every trigger silently goes inert. `NPC_VEZAX = 33271` undefined.
**F14.** No helper layer, no shared trigger/action resolver; `6.0f` duplicated, `2.0f` inline.
**F15.** `SPELL_VEZAX_SHADOW_CRASH = 63277` names the field, not the cast or the damage.
**F16.** Naming drift — `"... trigger"` / `"... action"` on both sides.

## Decisions

Full rework, staged. **The mana cheat is removed, not kept as a fallback**, following
`27fce75f9 fix(Uld/Mimiron): rebuild the strategy cheat-free`. The vapor puddle becomes the raid's
only mana source; if the soak underperforms, tune it — do not reinstate the cheat.

**Pattern source is Sunwell Plateau, not Ulduar.** Ulduar re-derives slots from a GUID rank every
tick, so a death reshuffles the formation mid-fight. SWP persists slots per instance.

| Decision | Choice |
|---|---|
| Hazard detection | **Poll**, not a registry. `GetDynamicObjectPositions(bot, r, 63277)` for the field, dead-creature scan on 33488 for the puddle. Plus a thin **stateless** observer whose only job is `RequestSpellInterrupt()` on bots caught inside. |
| Arc anchor | **Fixed room point** `(1852.78, 81.38, 342.461)`, KJ-style. Already verified, and a stable anchor is what makes slot-safety reassignment coherent. |
| Melee & tank | **Unanchored** on the boss, plus a light de-clump folded into the position action's `false` branch. |
| Healers | **Dedicated inner band**, so `PartyMemberToHeal`'s 30 yd measurement always reaches the tank. |
| Ring geometry | Healer **15 yd × 8 slots**; ranged inner **21 yd × 6**; ranged outer **28 yd × 6**; all over a **π** arc via `GetCenteredArcSlotAngleOffset`. |
| Field soak | **Bounded** — only casters whose slot is within ~one ring spacing of a live field move to it. Unbounded chasing collapses the arc into one 8 yd circle. |
| Vapor kills (normal) | **Every spawn**, by a **GUID-ranked subset of 2–3 ranged** using direct `Attack()`. **No skull mark** — `DpsTargetValue` prefers RTI and a stray skull is the whole raid (`ulduar.md:838-840`). |
| Puddle placement | **Kill in place.** Slot-safety reassignment handles a covered slot, and a puddle on the healer band is convenient. |
| Puddle exit | **HP-predictive** — leave when the next tick (`100 * 2^(stacks+1)`) would exceed a fraction of current health. Self-tunes across gear and raid size. Non-mana classes never soak. |
| Mark destination | **Three derived spots** off the arc bearing (left / centre / right), nearest wins, navprobe-verified. Travel time is the whole cost of the mechanic. |
| Searing Flames | Class-gate + exact-id cast detection + **GUID rank among capable *and ready* bots**, at `ACTION_EMERGENCY + 5`. Reliquary of Souls shape (`BT/BTTriggers.cpp:372`). |
| Surge of Darkness | **Tank defensive only.** The heal engine already reacts and the boss loses 55% move speed, so there is no positional component. |
| Animus | **Assist tank 0 taunts it**; main tank keeps Vezax pinned away from the berserk bounds. |
| Hard-mode protection | Targeting + AoE threat type + `CastDebuffSpellOnAttackerAction` + **explicit pet control** (`StopPet`, `RaidBossHelpers.cpp:417`). Tank cleave is already covered by the AoE threat type. |
| Encounter gate | **Presence-gated** resistance and reset; **combat-gated** everything else. Positioning and `VezaxControlMovementMultiplier` additionally require the bot to be inside the room (`VezaxFormationActive`): Vezax is visible from outside his hall, and a presence gate had bots prepositioning through walls before the pull while their generic movers were zeroed. Bubble is 45 yd around the anchor — the room door spawns at `(1854.86, 31.53)`, 49.9 yd out — plus a 10 yd height band. |

### SWP / BT pieces being reused

| Piece | Path | For |
|---|---|---|
| `GetCenteredArcSlotAngleOffset(slotIndex, slotCount, arcWidth)` | `SWP/Util/SWPEncounter_Brut.cpp:128` | Arc slots, filling outward from centre |
| `TryGetKiljaedenRangedSlotPosition` two-ring layout | `SWP/Util/SWPEncounter_KJ.cpp:216` | Independent rings, own slot counts |
| `EnsureKiljaedenRangedAssignments` | `SWP/Util/SWPEncounter_KJ.cpp:247` | Persistent per-instance slot map, prunes invalid holders |
| `IsKiljaedenRangedSlotSafe` + `EnsureKiljaedenRangedArmageddonAssignments` | `SWP/Util/SWPEncounter_KJ.cpp:103` | Temporary reassignment off a covered slot |
| `KiljaedenPositionRangedAction::Execute` | `SWP/Action/SWPActions_KJ.cpp:382` | Arrive-then-`return false` movement idiom |
| `ResolveMuruDpsTarget` | `SWP/Action/SWPActions_Muru.cpp:212` | Gather-to-struct, priority ladder, sticky current target |
| `MuruDisableDefaultTargetingMultiplier` | `SWP/SWPMultipliers.cpp:712` | Zeroes `DpsAssistAction`, `TankAssistAction`, `CastDebuffSpellOnAttackerAction` |
| `MuruControlMovementMultiplier` | `SWP/SWPMultipliers.cpp:761` | Family split first, then boss gate, then exemptions |
| `SunwellPlateauResetEncounterStatesAction` | `SWP/Action/SWPActions.cpp:22` | Clears per-instance state once the boss is gone |
| Reliquary Deaden trigger | `BT/BTTriggers.cpp:372` | Class-gate + `GetCurrentSpell` exact-id detection + EMERGENCY band |

## Implementation

### Stage 1 — Sev-1 correctness (independently shippable)

**Helper layer.** New `src/Ai/Raid/Uld/Util/UldEncounter_Vezax.{h,cpp}` mirroring
`SWPEncounter_KJ.{h,cpp}`, rather than growing the 1300-line `UldBossHelper.h`. Ids and tuned
constants stay in `UldBossHelper.h` beside the other bosses.

Ids: `NPC_VEZAX = 33271`, `SPELL_VEZAX_SHADOW_CRASH_CAST = 62660`,
`SPELL_VEZAX_SHADOW_CRASH_DMG = 62659`, `SPELL_VEZAX_SHADOW_CRASH_FIELD = 63277` (rename from
`SPELL_VEZAX_SHADOW_CRASH` — F15), `SPELL_VEZAX_SEARING_FLAMES = 62661`,
`SPELL_VEZAX_SURGE_OF_DARKNESS = 62662`, `SPELL_VEZAX_SARONITE_VAPORS_PUDDLE = 63322`,
`SPELL_VEZAX_SARONITE_BARRIER = 63364`. Delete `ULDUAR_VEZAX_PROFOUND_DARKNESS_RADIUS` (F2).

```cpp
struct VezaxHazard { Position position; float radius; bool isShadowCrashField; };
struct VezaxEncounterTargets { Unit* vezax; Unit* animus; std::vector<Unit*> liveVapors; };
```

`GatherVezaxHazards(Player*, std::vector<VezaxHazard>& out)` polls **once per action execution** —
`GetDynamicObjectPositions` is a grid search, and per-bot-per-tick across 25 bots is the cost
`docs/engine/raid-mechanics-lessons.md:135-163` warns about. Every slot check in that call reuses the
vector, matching the `IsKiljaedenRangedSlotSafe(position, hazards)` signature.

Also `GetVezax` by entry (F13), `VezaxEncounterActive`, `GatherVezaxEncounterTargets`,
`IsVezaxSlotSafe`.

**Invert Shadow Crash (F1)** — two nodes, both reading polled hazards, not the bot's own aura:
- `vezax shadow crash soak action` — mana-using ranged DPS (hunters excluded) whose slot is within
  ~one ring spacing of a live field move onto it and hold. Arrive, then `return false`.
- `vezax shadow crash clear action` — **healers only** step out of a field they are in. Melee and
  tanks are deliberately left in it: the field carries no damage, so it is a buff they cannot use
  rather than a hazard, and walking them out would spend uptime to avoid nothing. Keep the existing
  role bands (`MELEE 4-8`, `RANGED 13-17`) and the constant-arc step — a paladin healer can answer
  `IsMelee`, so the melee band is not dead code.

**Vapor puddle clear (F3)** — `vezax vapor puddle clear action`, HP-predictive exit, reading the
corpse scan.

**Delete Profound Darkness (F2)** — trigger, action, constant, node.

**Thin observer** — new `src/Ai/Raid/Uld/Util/UldBotScripts.cpp` plus `AddSC_UlduarBotScripts()`
in `src/Script/Playerbots.cpp`. Stateless, two `AllSpellScript::OnSpellCast` hooks: 62660 for the
Shadow Crash field, and **63323** — the aura a dying vapor casts on itself — for the puddle. 63323
fires exactly once at the instant the puddle appears, which is what makes the puddle half stateless
too; an `AllCreatureScript` death watch would have needed a seen-set. Each calls
`botAI->RequestSpellInterrupt()` on bots inside the radius **that the strategy is about to move**, so
a cast does not finish standing in a puddle at stack 6. **A missing script registration fails
silently** — verify it is reached.

### Stage 2 — positioning, mana economy, interrupt, Surge

**Slot machinery** in `UldEncounter_Vezax`: `VezaxEncounterState { rangedAssignments,
rangedDisplacedAssignments }` keyed by instance id; `EnsureVezaxRangedAssignments`,
`EnsureVezaxDisplacedAssignments`, `TryGetVezaxSlotPosition`.

**`vezax raid position action`** — `KiljaedenPositionRangedAction` shape: arrival check
(`GetExactDist2d <= 2.0f → return false`), else `MoveTo(..., MOVEMENT_COMBAT, lessDelay = true)`.
No slot (tank, melee) falls through to the de-clump. **Returning `false` once parked is
load-bearing** — it is what lets class interrupts at relevance 40 through.

Ring arithmetic to record in `ulduar.md`, the way `:295-318` does for Hodir: one ranged ring gives
6.9 yd spacing at 25-man; two rings give 12.6 and 17.6. The arc ends are 39 yd apart, but every DPS
slot has a healer slot within ~13 yd, and aggregate coverage is what `PartyMemberToHeal` needs.

**Mana economy (F5)** — `vezax kill vapor action` (GUID-ranked 2–3 ranged, direct `Attack()`, hard
mode off only) and `vezax vapor soak action` (below `lowMana`, healers first, hard mode off only).
Then **remove `vezax cheat trigger` / `vezax cheat action`** and their registrations.

**`vezax searing flames interrupt action`** — class-gate → `GetCurrentSpell(CURRENT_GENERIC_SPELL)`
id match on 62661 → GUID rank among capable-and-ready bots, recomputed per cast so cooldowns rotate
the duty naturally.

**`vezax surge of darkness action`** — tank fires a class defensive on `boss->HasAura(62662)`.

**Mark of the Faceless (F11)** — three derived spots off the arc bearing, nearest wins,
`MOVEMENT_COMBAT`. Drop the hardcoded corner and the inline `2.0f`.

**`vezax reset encounter state action`** at `ACTION_EMERGENCY + 10`, gated on Vezax being absent:
clears the instance entry (mechanic-tracker bot only) and the bot's own assignment. Without it the
assignment maps leak across pulls.

**`VezaxControlMovementMultiplier`** — family split first; zero `FollowAction`, `FleeAction`,
`CombatFormationMoveAction` for slotted roles; **exempt `AttackAction` and `ReachTargetAction`**
(`AttackAction : MovementAction`, not a sibling — `docs/engine/action-selection.md:18-36`); exempt
the encounter's own movers by `getName()`.

### Stage 3 — hard mode

**`VezaxDisableDefaultTargetingMultiplier`** — zero `DpsAssistAction` and `TankAssistAction` for the
whole encounter; with hard mode on, additionally zero `CastDebuffSpellOnAttackerAction` on a vapor
target and AoE cast actions (`getThreatType() == Action::ActionThreatType::Aoe`, pattern at
`Gruul/GruulMultipliers.cpp:103-108`) while a live vapor is in range. **The DoT half is the one that
matters** — a DoT ticking a vapor to death is the quietest way to lose hard mode.

**Pet control** — `StopPet(botAI)` when a vapor is the pet's target. A hunter pet off passive will
chew a wandering vapor with nobody noticing.

**Animus** — assist tank 0 taunts; everyone else keeps the existing direct-`Attack()` switch.

**Burst window (F9)** — Vezax arm in `UlduarBurstWindowMultiplier`: `allowLust` only once the Animus
is alive, and only with hard mode on.

### Node ladder (F12, F16)

Rewrite `UldStrategy.cpp:493-522` with a written rationale, using SWP's band split —
`ACTION_EMERGENCY + N` for dodges that kill, `ACTION_RAID + N` for positioning and targeting:

| Node | Priority |
|---|---|
| `vezax reset encounter state action` | `ACTION_EMERGENCY + 10` |
| `vezax mark of the faceless action` | `ACTION_EMERGENCY + 8` |
| `vezax vapor puddle clear action` | `ACTION_EMERGENCY + 7` |
| `vezax shadow crash clear action` | `ACTION_EMERGENCY + 6` |
| `vezax searing flames interrupt action` | `ACTION_EMERGENCY + 5` |
| `vezax surge of darkness action` | `ACTION_EMERGENCY + 4` |
| `vezax saronite animus action` | `ACTION_RAID + 3` |
| `vezax vapor soak action` | `ACTION_RAID + 2` |
| `vezax kill vapor action` | `ACTION_RAID + 1` |
| `vezax shadow crash soak action` | `ACTION_RAID + 1` |
| `vezax shadow resistance action` | `ACTION_RAID` |
| `vezax raid position action` | `ACTION_RAID` |

Rationale for the comment: Mark of the Faceless leads because it is the only node whose failure
*heals the boss* while draining the raid; the hazard-clears outrank the soaks because a bot already
dying does not need mana; the interrupt sits above everything in the RAID band so positioning cannot
starve it, but below the clears; position is last on purpose — every other node may interrupt it.

Rename to house convention (bare trigger name, `action`-suffixed action name).

## Files touched

| File | Change |
|---|---|
| `src/Ai/Raid/Uld/Util/UldEncounter_Vezax.h` / `.cpp` | **New.** Hazard polling, slot assignments, encounter-target struct, resolvers |
| `src/Ai/Raid/Uld/Util/UldBotScripts.cpp` | **New.** Raid-level scripts file, matching ICC/RS/SWP/TK. Stateless observers for `RequestSpellInterrupt` |
| `src/Script/Playerbots.cpp` | `AddSC_UlduarBotScripts()` declaration + call, alongside the six existing raids |
| `src/Ai/Raid/Uld/Util/UldBossHelper.h` | Vezax ids and constants; delete `ULDUAR_VEZAX_PROFOUND_DARKNESS_RADIUS` |
| `src/Ai/Raid/Uld/Trigger/UldTriggers_Vezax.h` / `.cpp` | Rewrite; drop Profound Darkness and the cheat trigger |
| `src/Ai/Raid/Uld/Action/UldActions_Vezax.h` / `.cpp` | Matching actions; drop the cheat and the Profound Darkness move-away |
| `src/Ai/Raid/Uld/UldTriggerContext.h`, `UldActionContext.h` | New names registered, dead ones removed |
| `src/Ai/Raid/Uld/UldStrategy.cpp` | Ranked ladder + rationale; register both multipliers |
| `src/Ai/Raid/Uld/UldMultipliers.h` / `.cpp` | `VezaxControlMovementMultiplier`, `VezaxDisableDefaultTargetingMultiplier`; Vezax arm in `UlduarBurstWindowMultiplier` |
| `docs/raids/ulduar.md` | New Vezax chapter; correct `:58-64` and `:845-853`; clear the gap rows at `:1150` and `:1166` |

**Every renamed trigger/action must change in all three places** — strategy node, trigger context,
action context — in the same edit. Name lookups fail silently (`docs/engine/pitfalls.md:9-29`).

## Verification

1. `python apps/codestyle/codestyle-cpp.py` before claiming done.
2. Build is a hand-off — the module cannot be compiled headless in the agent environment. Static
   review, then an explicit build request.
3. **navprobe every new coordinate** — three ring radii and three Mark spots. Client data lives in
   the `ac-client-data` Docker volume; `env/dist/data` being empty on the host is not a reason to
   skip it. Read the `settledZ` column, never the trailing "N/N on mesh" line.
4. Confirm the effective config with `docker exec ac-worldserver env | grep ^AC_` — never trust
   `playerbots.conf` for `UlduarVezaxHardMode`.
5. **Stage 1, 25-man:** casters sit in Shadow Crash fields; nobody rides a puddle past the predicted
   lethal tick; no bot walks off during Profound Darkness.
6. **Stage 2, 25-man:** the three rings form and hold; Searing Flames interrupted on every cast with
   exactly one kick spent; healers finish with mana and `BotCheats` stripped of `raid`.
7. **Stage 2, 10-man:** Searing Flames cadence is 15 s not 8 s — confirm the rotation does not idle.
   Slots under-fill from the arc centre outward, which is fine; confirm no bot is left unslotted.
8. **Stage 3, hard mode:** six vapors survive to the merge — watch DoT ticks and pets, not just
   direct attacks — the Animus spawns, assist tank 0 holds it, lust fires on it.
9. **State hygiene:** wipe and re-pull twice; assignments must not carry over. A leak shows up as
   bots walking to a slot nobody occupies.

## Build notes (as implemented)

Deviations from the design above, and facts found while writing it.

- **The arc-slot maths is local to `UldEncounter_Vezax.cpp`, not shared.** Promoting SWP's
  `GetCenteredArcSlotAngleOffset` into `RaidBossHelpers` was tried and reverted: SWP is stable and
  is not to be refactored for another raid's benefit. `VezaxArcSlotAngleOffset` is an
  independent implementation in the Vezax file's anonymous namespace. SWP is byte-identical to HEAD.
- **`MovementAction::FleePosition` cannot clear these hazards.** It clamps travel to
  `AiPlayerbot.FleeDistance`, which defaults to **5.0**, so it cannot walk a bot out of the middle of
  an 8 yd puddle. `vezax vapor puddle clear action` uses
  `FindNearestPositionClearOfHazards` instead (the warning is already on that declaration in
  `RaidBossHelpers.h`). The melee de-clump still uses `FleePosition` — its radius is 4 yd, inside the
  clamp.
- **`IsVezaxSpotSafe` takes the role-filtered `std::vector<Position>` avoid list**, not raw hazards:
  a mana caster wants to stand in a Shadow Crash field, so "dangerous" depends on who is asking.
  `VezaxBuildAvoidPositions` is what applies that filter.
- **`Trigger`'s `checkInterval` is milliseconds, not ticks** — the ctor multiplies anything under 100
  by 1000. `vezax shadow crash soak`, `vezax vapor soak` and `vezax raid position` use 2 (= 2 s) to
  keep the grid sweeps off the common tick; `vezax reset encounter state` uses 5.
- **Interrupt list excludes stuns.** Bash and Hammer of Justice cannot interrupt a boss. The list is
  kick, pummel, shield bash, counterspell, wind shear, mind freeze, silencing shot, spell lock.
- **Surge of Darkness uses tank cooldowns only** — no Divine Shield or Ice Block, which would shed
  the boss the tank is holding away from the berserk boundary.

### Navprobe results (map 603, anchor 1852.78 / 81.3856 / 342.461)

The floor is a WMO: raw terrain reads −27.7, `Map::GetHeight` reads ~342.378. Read `settledZ`.

| Probe | Result |
|---|---|
| Ring r=15, 16 headings | 16/16 on mesh, settledZ 342.34–342.47 |
| Ring r=21, 16 headings | 16/16 on mesh, settledZ 342.28–342.52 |
| Ring r=28, 16 headings | 16/16 on mesh, settledZ 341.87–342.56 |
| Mark NE (1871.05, 99.89) | poly dist 0.047, settledZ 342.379 |
| Mark NW (1833.03, 98.30) | poly dist 0.047, settledZ 342.378 |
| Mark N  (1851.70, 107.36) | poly dist 0.091, settledZ 342.378 |

Arc orientation −1.5291 rad is the bearing from Vezax' spawn to his door (GO 194750 at y 31.5), i.e.
due south. The raid therefore fills the southern half and the three Mark spots sit in the empty
northern half, ~20 yd clear of the melee stack and ~20 yd clear of the nearest arc slot.

## Status

- [x] Stage 1 — Sev-1 correctness (written, not yet compiled or tested in-game)
- [x] Stage 2 — positioning, mana economy, interrupt, Surge (written, not yet compiled or tested in-game)
- [ ] Stage 3 — hard mode

Delete this file and fold the durable findings into `docs/raids/ulduar.md` when all three stages
ship (`docs/raids/README.md:173-178`).
