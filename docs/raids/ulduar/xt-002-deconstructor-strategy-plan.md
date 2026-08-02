# XT-002 Deconstructor playerbot strategy (Ulduar)

## Context

Ulduar (`src/Ai/Raid/Uld/`) has bot strategies for 13 of its 14 encounters. **XT-002 Deconstructor is the
only one with zero code** — grep for `XT002|Deconstructor|Heartbreak|Tympanic|Gravity Bomb|Boombot|
Scrapbot|Life Spark` across the module returns nothing but a "not implemented" line in
`docs/raids/ulduar/ulduar-file-structure-refactor-plan.md:13`. Bots pulled into that room today fall back
to generic combat: they stand in Gravity Bomb, melee Boombots, ignore the exposed Heart, and blow
Bloodlust on the pull.

This plan adds the full encounter: phase-1 mechanics, the Heart window, hard mode behind a config gate,
a burst-cooldown hold that releases at the right moment per mode, and hunter/rogue threat redirection.

**First implementation step:** copy this plan to
`modules/mod-playerbots/docs/raids/ulduar/xt002-deconstructor-strategy-plan.md` (the raid docs live in the
repo, not in the plans temp dir), then work from there.

---

## Encounter facts (verified against this fork's core script)

Source: `src/server/scripts/Northrend/Ulduar/Ulduar/boss_xt002.cpp`. Ulduar is 10/25-man only — there is
**no heroic difficulty**, so "normal vs hard mode" is the Heartbreak split, not a difficulty flag.

**Phase 1 (`PHASE_1`)** — plain tank-and-spank plus three timed mechanics:
- **Searing Light** (`63018` 10-man / `65121` 25-man): debuff on a random player, AoE damage around them.
  On expiry **in hard mode only** XT summons a Life Spark on that player (`spell_xt002_searing_light_spawn_life_spark`,
  gated on XT having the Heartbreak aura).
- **Gravity Bomb** (`63024` / `64234`): DoT + AoE damage around the carrier. On expiry **in hard mode only**
  it drops a Void Zone (`spell_xt002_gravity_bomb_aura`, same Heartbreak gate).
- **Tympanic Tantrum** (`62776`, every 60s): raid-wide % max-health damage to players and pets. Nothing to
  dodge — existing heal strategies cover it. No bot handling needed.

**Heart phase (`PHASE_HEART`)** — at 75%, 50% and 25% XT submerges: `REACT_PASSIVE`, `AttackStop()`,
`UNIT_FLAG_NOT_SELECTABLE` set (`boss_xt002.cpp:386-393`). The Heart of the Deconstructor becomes
selectable and channels **Exposed Heart** (`63849`) for 30s. Damage to the Heart transfers to XT
(`SetData(DATA_TRANSFERED_HEALTH)`, `boss_xt002.cpp:947-963`) and the aura is a
`SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN` — this window is the encounter's real damage multiplier.

**Hard mode** — killing the Heart during the window fires `ACTION_ENTER_HARD_MODE`
(`npc_xt002_heart::JustDied`, `boss_xt002.cpp:462-466`): XT is set to **full health**, gains
**Heartbreak** (`65737`) permanently, and `EVENT_PHASE_CHECK` is never rescheduled — so **there are no
further Heart phases**. The Heartbreak aura is a reliable runtime signal that hard mode is live.

**Adds** — spawned from XT-Toy Piles when the Heart's overload fires an Energy Orb at one
(`npc_xt_toy_pile::SpellHit`): 5-7 Scrapbots, one Boombot, 30% chance of a Pummeller.
- **XS-013 Scrapbot** — walks to XT and heals him (`SPELL_SCRAP_REPAIR`). Must die en route.
- **XE-321 Boombot** — explodes for 15-18k (`SPELL_BOOM 62834`) on reaching XT **or at 50% health**
  (`npc_boombot::DamageTaken`, `boss_xt002.cpp:632-640`). Melee must never touch it.
- **XM-024 Pummeller** — aggressive add with Trample/Arcing Smash/Uppercut. Wants an off-tank.
- **Life Spark** / **Void Zone** — hard-mode only, described above.

> **Fork quirk, deliberate:** `npc_xt_toy_pile::SummonDistance = 90.0f` and the check is
> `if (!xt002 || xt002->IsWithinDist(me, SummonDistance)) return;` (`boss_xt002.cpp:837-848`) — adds only
> spawn when XT is **more than** 90 yd from the pile. Tanked in place, adds essentially never appear on
> this core. Per decision: **do not touch tank positioning**. Write the add handling so it works whenever
> adds do spawn, and leave this note in the doc.

### IDs

`NPC_XT002 (33293)`, `NPC_XT_TOY_PILE (33337)`, `NPC_XS013_SCRAPBOT (33343)` and
`NPC_HEART_OF_DECONSTRUCTOR (33329)` already come from core `ulduar.h:161-164`, which is reachable via
`Uld/Util/UldScripts.h` — **do not redeclare them** (matches the existing convention noted at
`UldBossHelper.h:36,70,211`).

New ids for `UldBossHelper.h` (confirmed against `data/sql/base/db_world/creature_template.sql` and
`data/sql/updates/db_world/2026_07_04_03.sql`, which fixes the add ScriptNames on this fork):

| Name | Value |
|---|---|
| `PB_NPC_XT002_PUMMELLER` | 33344 |
| `PB_NPC_XT002_BOOMBOT` | 33346 |
| `PB_NPC_XT002_LIFE_SPARK` | 34004 |
| `PB_NPC_XT002_VOID_ZONE` | 34001 |
| `SPELL_XT002_SEARING_LIGHT_10` / `_25` | 63018 / 65121 |
| `SPELL_XT002_GRAVITY_BOMB_10` / `_25` | 63024 / 64234 |
| `SPELL_XT002_EXPOSED_HEART` | 63849 |
| `SPELL_XT002_HEARTBREAK` | 65737 |
| `SPELL_XT002_SUBMERGE` | 37751 |

Tuning constants (same style as the existing `ULDUAR_*` block at `UldBossHelper.h:228-284`, each with a
one-line rationale):

```
ULDUAR_XT002_DEBUFF_SPREAD_RADIUS      = 12.0f   // Searing Light / Gravity Bomb splash
ULDUAR_XT002_BOOMBOT_AVOID_RADIUS      = 12.0f   // Boom is ~10 yd; leave margin for the 50% HP trigger
ULDUAR_XT002_VOID_ZONE_RADIUS          =  6.0f
ULDUAR_XT002_HEART_SAFE_HP_PCT         = 15.0f   // normal mode: stop DPS above this floor
ULDUAR_XT002_FINAL_PUSH_HP_PCT         = 25.0f   // normal mode: burst release below this
```

---

## Config gate

Follow the existing eight `Ulduar*HardMode` options exactly, three sites:

1. `src/PlayerbotAIConfig.h:267-275` — add `bool ulduarXT002HardMode;` to the block.
2. `src/PlayerbotAIConfig.cpp:692-700` — `ulduarXT002HardMode = sConfigMgr->GetOption<bool>("AiPlayerbot.UlduarXT002HardMode", false);`
3. `conf/playerbots.conf.dist` (insert in encounter order, near `:372`) — prose block, `# Default: 0 (disabled)`, key.

Conf text must say plainly: with the option **on**, bots burn the exposed Heart to zero on the first
window to trigger Heartbreak; with it **off**, bots damage the Heart but stop at 15% so the fight stays
in normal mode. It must match what the raid actually intends — bots do not detect intent.

`Uld/Util/UldHardMode.{h,cpp}` gets two functions:

```cpp
// Declares intent: the raid means to kill the Heart. There is no pre-kill server signal to confirm
// against, so this is the config alone.
bool IsXT002HardModeActive(PlayerbotAI* botAI);   // return sPlayerbotAIConfig.ulduarXT002HardMode;

// Hard mode is already live - XT carries Heartbreak, there will be no further Heart phase, and
// Life Sparks / Void Zones now spawn. True regardless of the config, so a human-triggered
// Heartbreak is handled too.
bool IsXT002HeartbreakActive(PlayerbotAI* botAI);
```

---

## Files

New, matching the per-boss layout (no CMake edits — the core globs `src/` recursively):

- `src/Ai/Raid/Uld/Trigger/UldTriggers_XT002.{h,cpp}` — add to the `UldTriggers.h` umbrella
- `src/Ai/Raid/Uld/Action/UldActions_XT002.{h,cpp}` — add to the `UldActions.h` umbrella

Modified:

- `Uld/Util/UldBossHelper.h` (+ `.cpp`) — ids, constants, encounter helpers
- `Uld/Util/UldHardMode.{h,cpp}` — the two functions above
- `Uld/UldTriggerContext.h` / `UldActionContext.h` — `creators[...]` + one static factory each
- `Uld/UldStrategy.cpp` — trigger nodes in encounter order (XT-002 sits **after Ignis, before Iron
  Assembly**, ~line 85), plus two multipliers in `InitMultipliers` (`:478-482`)
- `Uld/UldMultipliers.{h,cpp}` — two new multiplier classes
- `src/PlayerbotAIConfig.{h,cpp}`, `conf/playerbots.conf.dist`

No changes to `RaidStrategyContext.h`, `BuildShared*Contexts.cpp` or `PlayerbotAI.cpp` — the `"ulduar"`
strategy is already registered and auto-selected by map id 603 (`PlayerbotAI.cpp:1729`).

### Encounter helpers (`UldBossHelper.cpp`, declared in the `.h` next to `GetAlgalonBigBangSoakerPriest`)

```cpp
Unit* GetXT002(PlayerbotAI* botAI);            // GetFirstAliveUnitByEntry(botAI, NPC_XT002)
Unit* GetXT002ExposedHeart(PlayerbotAI* botAI);// alive heart entry 33329 carrying aura 63849
bool  IsXT002Submerged(PlayerbotAI* botAI);    // XT has UNIT_FLAG_NOT_SELECTABLE or aura 37751
uint32 GetXT002SearingLightSpellId(Player* bot); // 25-man -> 65121 else 63018
uint32 GetXT002GravityBombSpellId(Player* bot);  // 25-man -> 64234 else 63024
Unit* GetXT002KillTarget(PlayerbotAI* botAI);  // Life Spark > Scrapbot > Boombot > Pummeller
```

Use `GetFirstAliveUnitByEntry` (`RaidBossHelpers.cpp:220`, scans `"possible targets no los"`) rather than
`AI_VALUE2(Unit*, "find target", ...)`. `FindTargetValue::Calculate` (`TargetValue.cpp:160-185`) only walks
the bot's own threat list **and requires an exact full-name match** — the Heart never attacks anyone, so
`find target` would never resolve it.

---

## Trigger / action nodes

Every trigger short-circuits on `GetXT002(botAI) == nullptr` first, so the shared Ulduar strategy stays
inert during the other twelve bosses. Priority constants from `src/Bot/Engine/Strategy/Strategy.h:53-67`.

| Trigger node | Action | Priority | Fires when |
|---|---|---|---|
| `xt002 searing light spread trigger` | `xt002 searing light spread action` | `ACTION_EMERGENCY` | Another raid member within 12 yd carries Searing Light |
| `xt002 gravity bomb spread trigger` | `xt002 gravity bomb spread action` | `ACTION_EMERGENCY` | Another raid member within 12 yd carries Gravity Bomb |
| `xt002 gravity bomb carrier trigger` | `xt002 gravity bomb carrier action` | `ACTION_EMERGENCY + 1` | **This bot** carries Gravity Bomb — run ≥12 yd clear of the nearest raid member (in hard mode this is also where the Void Zone lands) |
| `xt002 boombot avoid trigger` | `xt002 boombot avoid action` | `ACTION_EMERGENCY` | Melee bot within 12 yd of a Boombot |
| `xt002 void zone trigger` | `xt002 void zone action` | `ACTION_EMERGENCY` | Within 6 yd of Void Zone 34001 |
| `xt002 mark kill target trigger` | `xt002 mark kill target action` | `ACTION_RAID` | Marker bot only; skull on `GetXT002KillTarget` |
| `xt002 attack kill target trigger` | `attack rti target` | `ACTION_RAID + 1` | Skull is set on a live XT add |
| `xt002 boombot ranged kill trigger` | `xt002 boombot ranged kill action` | `ACTION_RAID + 2` | Ranged bot, Boombot alive ≥12 yd away |
| `xt002 attack heart trigger` | `xt002 attack heart action` | `ACTION_RAID + 3` | Heart exposed, no higher-priority add, and (hard mode **or** heart HP > 15%) |
| `xt002 pummeller taunt trigger` | `xt002 pummeller taunt action` | `ACTION_RAID + 2` | Tank bot, Pummeller alive and not on a tank |
| `xt002 redirect threat trigger` | `xt002 redirect threat action` | `ACTION_RAID + 1` | Hunter/rogue, XT up and not submerged, redirect ready |

Reuse, don't reinvent:
- `TooCloseToCreatureTrigger::TooCloseToCreature(entry, radius)` (`RangeTriggers.cpp:214`) +
  `MoveAwayFromCreatureAction` (`MovementActions.h:305`) for Boombot and Void Zone — same shape as
  `UldTriggers_Ignis.cpp:27-28` / `UldActions_Ignis.h:21-22`.
- `TooCloseToPlayerWithDebuffTrigger::TooCloseToPlayerWithDebuff(spellId, range)`
  (`RangeTriggers.cpp:220`) for the two spread triggers — call it once per difficulty id and OR the
  results.
- `MoveAwayFromPlayerWithDebuffAction` (`MovementActions.cpp:2922`) takes a **single** spell id fixed at
  construction, so it cannot cover both the 10- and 25-man ids. Add one small
  `XT002MoveAwayFromDebuffedAction` in `UldActions_XT002.cpp` that resolves the id via
  `GetXT002SearingLightSpellId` / `GetXT002GravityBombSpellId` and otherwise mirrors that 8-direction
  safe-spot scan.
- Marking / focus: `IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID)` + `SetRtiTarget(botAI, "skull", …)`
  (`RaidBossHelpers.h:24,26`), then the generic `"attack rti target"` action — exactly the Kologarn
  pattern (`UldStrategy.cpp:125-131`).
- Pummeller taunt: copy the class switch at `UldActions_YoggSaron.cpp:595-607`
  (warrior `taunt` / paladin `hand of reckoning` / DK `dark command` / druid `growl`).

The skull channel belongs to the adds; the Heart is attacked directly by
`XT002AttackHeartAction : AttackAction` so the two never fight over the mark.

---

## Burst-cooldown suppression

`XT002BurstWindowMultiplier` in `UldMultipliers.{h,cpp}`, modelled on `NaxxBurstWindowMultiplier`
(`NaxxMultipliers.cpp:596-610`) — including its one-tick `cachedAtMs` / `cachedValue` memo, since this
multiplier runs against every candidate action.

Match by name via `IsBurstCooldownAction(action->getName())` from `src/Ai/Base/Combat/BurstCooldowns.h:21`.
That registry (`BurstCooldowns.cpp:22-46`) already covers `"bloodlust"`, `"heroism"`, every class burst
cooldown, racials, `"use trinket"` and `"offensive potion"` — do **not** hand-roll a `dynamic_cast` list
the way BT/SWP do.

```
if (!IsBurstCooldownAction(name))          -> 1.0f
if (!GetXT002(botAI))                      -> 1.0f   // not this encounter
if (IsXT002HeartbreakActive(botAI))        -> 1.0f   // hard mode live: no more Heart phases, spend it
if (GetXT002ExposedHeart(botAI))
    -> IsXT002HardModeActive(botAI) ? 1.0f : 0.0f    // burn the Heart only when hard mode is intended
if (!IsXT002HardModeActive(botAI) && xt->GetHealthPct() < 25.0f) -> 1.0f   // normal mode final push
-> 0.0f
```

So: **hard mode on** → everything held until the first Heart exposure, then released for good once
Heartbreak lands. **Hard mode off** → held through all three Heart phases, released when XT drops below
25% (after the last Heart closes). This composes with the existing global
`HoldBurstUntilTankEngagedMultiplier` (`BurstWindowStrategy.cpp:12-57`); both are multiplied, no conflict.

## Heart safety floor (normal mode)

Same multiplier class or a second one, `XT002HeartFloorMultiplier` — the condition-based shape of
`AlgalonMultiplier` (`UldMultipliers.cpp:12-31`):

> When hard mode is **off**, the Heart is exposed, and it is at or below `ULDUAR_XT002_HEART_SAFE_HP_PCT`,
> return `0.0f` for any action whose `AI_VALUE(Unit*, "current target")` is the Heart.

Belt and braces: `xt002 attack heart trigger` also goes false at the floor, and the marker bot clears the
skull. The multiplier is what actually stops in-flight damage from bots that already have the Heart
targeted — without it a bot keeps swinging and flips the raid into hard mode by accident.

## Misdirection / Tricks of the Trade

`XT002RedirectThreatAction` — copy the ~30-line body of `NaxxRedirectThreatAction::Execute`
(`NaxxActions_Shared.cpp:12-45`) into `UldActions_XT002.cpp` rather than reaching across raid
directories:

```
isUseful(): bot is CLASS_HUNTER or CLASS_ROGUE
target:     Pummeller alive           -> GetGroupAssistTank(botAI, bot, 0)
            else tank currently holding XT (group tank whose GUID == xt->GetVictim())
            else GetGroupMainTank(botAI, bot)
rogue:      CanCastSpell("tricks of the trade", tank) && CastSpell(...)
hunter:     CanCastSpell("misdirection", tank) ? cast
            : has aura 35079 && CanCastSpell("steady shot", xt) -> spend a charge on XT
```

Trigger: XT alive and not submerged, bot is hunter/rogue, and the redirect is off cooldown (let
`CanCastSpell` do the readiness check, as ZA does at `ZAActions.cpp:45-50`). No phase memory needed —
firing whenever the ~30s cooldown is up keeps it rolling on the tank through the pull and every
post-Heart resume, which is the correct play anyway.

Add to `XT002BurstWindowMultiplier` (or a small `XT002ThreatRedirectMultiplier`, mirroring
`ZAMultipliers.cpp:105-115`): while XT is alive, return `0.0f` for
`dynamic_cast<CastMisdirectionOnMainTankAction*>` and `dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>`
so the class-generic low-tank-threat versions don't spend the cooldown on the wrong tank.

---

## Behaviour matrix

| | `UlduarXT002HardMode = 0` | `UlduarXT002HardMode = 1` |
|---|---|---|
| Heart exposed | Attack it, stop at 15% HP | Burn to zero |
| Bloodlust / burst | Held until XT < 25% | Held until first Heart exposure, then free |
| Life Spark / Void Zone | Never spawn; triggers stay inert | Handled once Heartbreak is up |
| Adds, spread, redirect | Identical in both modes | Identical in both modes |

---

## Code style

CLAUDE.md hard rules: 4-space indent, Allman braces, `auto const&`, `Type const*`, `{}` fmt specifiers,
`LOG_*`, project `urand`/`frand`, `ObjectGuid` (never a raw `Player*`) across ticks. Run
`python apps/codestyle/codestyle-cpp.py` before calling it done.

## Verification

The module cannot be compiled headless here — static checks, then hand off the build.

1. `git grep -n "xt002" modules/mod-playerbots/src` — every trigger node name in `UldStrategy.cpp` has a
   matching `creators[...]` entry in `UldTriggerContext.h`, and every action name one in
   `UldActionContext.h`. **Check the spelling character by character**: `UldTriggerContext.h:82` already
   has a live typo (`"…shadow resistance trigger**r**"`) whose node at `UldStrategy.cpp:369` never
   resolves.
2. `git grep -n "33293\|33329\|33337\|33343" modules/mod-playerbots/src/Ai/Raid/Uld` — should be empty;
   those four come from core `ulduar.h`.
3. Confirm `IsXT002HardModeActive` is a bare config read and `IsXT002HeartbreakActive` reads aura 65737
   off XT with no config check.
4. Build the core with `mod-playerbots` compiled in; hand off if no local toolchain.
5. In-game, `UlduarXT002HardMode = 0`: pull XT with a bot raid. Expect — no lust on the pull; bots spread
   out of Searing Light / Gravity Bomb; the Gravity Bomb carrier runs clear; the Heart gets attacked at
   each of 75/50/25% and damage stops around 15%; XT **never** gains Heartbreak; lust and cooldowns all
   go off below 25%.
6. Same pull with `UlduarXT002HardMode = 1`: lust fires the moment the Heart is exposed at 75%, the Heart
   dies, XT full-heals with Heartbreak, and from then on bots dodge Void Zones and focus Life Sparks.
7. Adds: they only spawn if XT is dragged >90 yd from a Toy Pile. To exercise the add handling, pull XT
   and walk him to the door — melee should refuse to touch Boombots, ranged should kill them at distance,
   skull should land on Scrapbots, and a tank bot should taunt the Pummeller.
8. Regression: run one other Ulduar boss (Ignis is next door) and confirm nothing XT-related fires and
   Bloodlust is unaffected.
